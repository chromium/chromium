// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.app.Activity;
import android.content.ClipData;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Process;
import android.provider.DocumentsContract;
import android.provider.MediaStore;
import android.webkit.MimeTypeMap;

import androidx.core.content.ContextCompat;

import org.jni_zero.JniRawPtr;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowMimeTypeMap;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.FileProviderUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.FileUtilsJni;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.ui.base.SelectFileDialog.NativeSelectFileDialogImpl;
import org.chromium.ui.permissions.PermissionCallback;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Tests logic in the SelectFileDialog class. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({UiAndroidFeatures.DISABLE_PHOTO_PICKER_FOR_VIDEO_CAPTURE})
public class SelectFileDialogTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public TemporaryFolder mTempFolder = new TemporaryFolder();

    // A callback that fires when the file selection pipeline shuts down as a result of an action.
    public final CallbackHelper mOnActionCallback = new CallbackHelper();

    @Mock FileUtils.Natives mFileUtilsMocks;

    @Before
    public void setUp() {
        SelectFileDialog.clearActiveDialogsForTesting();
        // Emulate realpath(): canonicalize existing paths, return "" for nonexistent ones.
        FileUtilsJni.setInstanceForTesting(mFileUtilsMocks);
        Mockito.lenient()
                .doAnswer(invocation -> realpath(invocation.getArgument(0)))
                .when(mFileUtilsMocks)
                .getAbsoluteFilePath(any());
    }

    private static String realpath(String path) {
        File file = new File(path);
        if (!file.exists()) return "";
        try {
            return file.getCanonicalPath();
        } catch (IOException e) {
            return "";
        }
    }

    private void runAllAsyncTasks() {
        RobolectricUtil.runAllBackgroundAndUi();
    }

    /** Argument matcher that matches Intents with the same action. */
    private static class IntentArgumentMatcher implements ArgumentMatcher<Intent> {
        private final String mAction;

        public IntentArgumentMatcher(String action) {
            mAction = action;
        }

        @Override
        public boolean matches(Intent other) {
            return mAction.equals(other.getAction());
        }

        @Override
        public String toString() {
            return mAction;
        }
    }

    private class TestSelectFileDialog extends SelectFileDialog {
        // Counts how often the upload attempts are aborted.
        private int mFileSelectionAborted;

        // Counts how often the upload results in some files being uploaded.
        private int mFileSelectionSuccess;

        TestSelectFileDialog(JniRawPtr<NativeSelectFileDialogImpl> nativeDialog) {
            super(nativeDialog);
            mFileSelectionSuccess = 0;
        }

        @Override
        protected void onFileSelected(
                JniRawPtr<NativeSelectFileDialogImpl> nativeSelectFileDialogImpl,
                String filePath,
                String displayName) {
            mFileSelectionSuccess++;
            mOnActionCallback.notifyCalled();
        }

        @Override
        protected void onMultipleFilesSelected(
                JniRawPtr<NativeSelectFileDialogImpl> nativeSelectFileDialogImpl,
                String[] filePathArray,
                String[] displayNameArray) {
            mFileSelectionSuccess++;
            mOnActionCallback.notifyCalled();
        }

        @Override
        protected void onFileNotSelected(
                JniRawPtr<NativeSelectFileDialogImpl> nativeSelectFileDialogImpl) {
            mFileSelectionAborted++;
            mOnActionCallback.notifyCalled();
        }

        private void resetFileSelectionAttempts() {
            mFileSelectionAborted = 0;
            mFileSelectionSuccess = 0;
            // Clear that state here so the next selectFile() is not rejected as a concurrent
            // request.
            SelectFileDialog.clearActiveDialogsForTesting();
        }
    }

    public void testMimeTypesWithExternalPicker(String intentAction) throws Exception {
        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);

        // Select a simple (non-media) MIME type without setting up successful intent handling, to
        // simulate the pipeline aborting because showIntent fails.
        int callCount = mOnActionCallback.getCallCount();
        selectFileDialog.selectFile(
                intentAction,
                new String[] {"application/pdf"},
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);
        mOnActionCallback.waitForCallback(callCount, 1);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(1, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();

        // Now setup WindowAndroid#showIntent to succeed for our next run.
        IntentArgumentMatcher chooserIntentArgumentMatcher =
                new IntentArgumentMatcher(Intent.ACTION_CHOOSER);
        Mockito.doAnswer(
                        (invocation) -> {
                            // When showIntent is called, we use the opportunity to check on the
                            // values we expect to see within the Intent data.
                            Intent chooserIntent = (Intent) invocation.getArguments()[0];
                            Intent getContentIntent =
                                    (Intent) chooserIntent.getExtra(Intent.EXTRA_INTENT);
                            assertEquals(
                                    null, getContentIntent.getExtra(Intent.EXTRA_ALLOW_MULTIPLE));
                            assertEquals("*/*", getContentIntent.getType());
                            String[] mimeTypes =
                                    (String[]) getContentIntent.getExtra(Intent.EXTRA_MIME_TYPES);
                            assertArrayEquals(new String[] {"application/pdf"}, mimeTypes);
                            assertEquals(
                                    null, getContentIntent.getExtra(Intent.EXTRA_INITIAL_INTENTS));
                            assertTrue(getContentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
                            return true;
                        })
                .when(windowAndroid)
                .showIntent(
                        ArgumentMatchers.argThat(chooserIntentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        // Simulate showing the dialog, allowing a PDF to be uploaded and watch the pipeline
        // remain open.
        callCount = mOnActionCallback.getCallCount();
        selectFileDialog.selectFile(
                intentAction,
                new String[] {"application/pdf"},
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(0, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();

        // Setup showIntent to check for slightly different values for our next run.
        Mockito.doAnswer(
                        (invocation) -> {
                            Intent chooserIntent = (Intent) invocation.getArguments()[0];
                            Intent getContentIntent =
                                    (Intent) chooserIntent.getExtra(Intent.EXTRA_INTENT);
                            assertEquals(
                                    true, getContentIntent.getExtra(Intent.EXTRA_ALLOW_MULTIPLE));
                            assertEquals("*/*", getContentIntent.getType());
                            String[] mimeTypes =
                                    (String[]) getContentIntent.getExtra(Intent.EXTRA_MIME_TYPES);
                            // Adding a media related MIME-type adds an extra MIME type to avoid
                            // ACTION_GET_CONTENT hijacking.
                            assertArrayEquals(
                                    new String[] {
                                        "application/pdf", "image/gif", "type/nonexistent"
                                    },
                                    mimeTypes);
                            assertEquals(
                                    null, getContentIntent.getExtra(Intent.EXTRA_INITIAL_INTENTS));
                            assertTrue(getContentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
                            return true;
                        })
                .when(windowAndroid)
                .showIntent(
                        ArgumentMatchers.argThat(chooserIntentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        // Add a media file to the mix and allow multiple files.
        callCount = mOnActionCallback.getCallCount();
        selectFileDialog.selectFile(
                intentAction,
                new String[] {"application/pdf", "image/gif"},
                /* capture= */ false,
                /* multiple= */ true,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(0, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();
    }

    @Test
    public void testMimeTypesWithExternalPicker() throws Exception {
        testMimeTypesWithExternalPicker(Intent.ACTION_GET_CONTENT);
    }

    @Test
    public void testExternalPickerWithFileExtensions() throws Exception {
        verifyExternalPickerWithFileExtensions(Intent.ACTION_GET_CONTENT);
    }

    public void verifyFileSystemAccessIntent(String intentAction) throws Exception {
        ShadowMimeTypeMap shadowMimeTypeMap = Shadows.shadowOf(MimeTypeMap.getSingleton());
        shadowMimeTypeMap.addExtensionMimeTypeMapping("jpg", "image/jpeg");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("png", "image/png");

        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        String[] fileTypes = {"image/jpeg", "image/png"};

        // Setup WindowAndroid#showIntent to succeed (and validate the call).
        IntentArgumentMatcher intentArgumentMatcher = new IntentArgumentMatcher(intentAction);
        Mockito.doAnswer(
                        (invocation) -> {
                            // Validate intent.
                            Intent intent = (Intent) invocation.getArguments()[0];
                            assertEquals(null, intent.getExtra(Intent.EXTRA_INTENT));
                            if (Intent.ACTION_OPEN_DOCUMENT.equals(intentAction)) {
                                assertEquals("*/*", intent.getType());
                                assertEquals(true, intent.getExtra(Intent.EXTRA_ALLOW_MULTIPLE));
                                assertArrayEquals(
                                        fileTypes,
                                        (String[]) intent.getExtra(Intent.EXTRA_MIME_TYPES));
                                assertEquals(null, intent.getExtra(Intent.EXTRA_TITLE));
                                assertTrue(intent.hasCategory(Intent.CATEGORY_OPENABLE));
                            } else if (Intent.ACTION_OPEN_DOCUMENT_TREE.equals(intentAction)) {
                                assertEquals(null, intent.getType());
                                assertEquals(null, intent.getExtra(Intent.EXTRA_ALLOW_MULTIPLE));
                                assertEquals(null, intent.getExtra(Intent.EXTRA_MIME_TYPES));
                                assertEquals(null, intent.getExtra(Intent.EXTRA_TITLE));
                                assertFalse(intent.hasCategory(Intent.CATEGORY_OPENABLE));
                            } else if (Intent.ACTION_CREATE_DOCUMENT.equals(intentAction)) {
                                assertEquals("image/jpeg", intent.getType());
                                assertEquals(null, intent.getExtra(Intent.EXTRA_ALLOW_MULTIPLE));
                                assertEquals(null, intent.getExtra(Intent.EXTRA_MIME_TYPES));
                                assertEquals("suggested.txt", intent.getExtra(Intent.EXTRA_TITLE));
                                assertTrue(intent.hasCategory(Intent.CATEGORY_OPENABLE));
                            } else {
                                fail("unknown intent " + intentAction);
                            }
                            assertEquals(
                                    Uri.parse("content://authority/tree/123"),
                                    intent.getExtra(DocumentsContract.EXTRA_INITIAL_URI));
                            if (Intent.ACTION_CREATE_DOCUMENT.equals(intentAction)) {
                                assertEquals("suggested.txt", intent.getExtra(Intent.EXTRA_TITLE));
                            } else {
                                assertEquals(null, intent.getExtra(Intent.EXTRA_TITLE));
                            }
                            return true;
                        })
                .when(windowAndroid)
                .showIntent(
                        ArgumentMatchers.argThat(intentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        // Simulate showing the dialog, allowing a directory to be selected.
        selectFileDialog.selectFile(
                intentAction,
                fileTypes,
                /* capture= */ false,
                /* multiple= */ true,
                /* defaultDirectory= */ "content://authority/tree/123",
                /* suggestedName= */ "suggested.txt",
                windowAndroid);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(0, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();
    }

    @Test
    public void testFileSystemAccessOpenDocument() throws Exception {
        verifyFileSystemAccessIntent(Intent.ACTION_OPEN_DOCUMENT);
    }

    @Test
    public void testFileSystemAccessCreateDocument() throws Exception {
        verifyFileSystemAccessIntent(Intent.ACTION_CREATE_DOCUMENT);
    }

    private void verifyCreateDocumentIntent(
            String[] fileTypes,
            String suggestedName,
            String expectedMimeType,
            String expectedTitle) {
        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);

        IntentArgumentMatcher intentArgumentMatcher =
                new IntentArgumentMatcher(Intent.ACTION_CREATE_DOCUMENT);
        Mockito.doAnswer(
                        (invocation) -> {
                            Intent intent = (Intent) invocation.getArguments()[0];
                            assertEquals(expectedMimeType, intent.getType());
                            assertEquals(expectedTitle, intent.getExtra(Intent.EXTRA_TITLE));
                            assertTrue(intent.hasCategory(Intent.CATEGORY_OPENABLE));
                            return true;
                        })
                .when(windowAndroid)
                .showIntent(
                        ArgumentMatchers.argThat(intentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        selectFileDialog.selectFile(
                Intent.ACTION_CREATE_DOCUMENT,
                fileTypes,
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                suggestedName,
                windowAndroid);
    }

    @Test
    public void testCreateDocumentInfersMimeTypeFromSuggestedName() throws Exception {
        ShadowMimeTypeMap shadowMimeTypeMap = Shadows.shadowOf(MimeTypeMap.getSingleton());
        shadowMimeTypeMap.addExtensionMimeTypeMapping("webp", "image/webp");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("gz", "application/gzip");

        verifyCreateDocumentIntent(new String[] {}, "images.webp", "image/webp", "images.webp");
        verifyCreateDocumentIntent(new String[] {}, "my photo.webp", "image/webp", "my photo.webp");
        verifyCreateDocumentIntent(
                new String[] {}, "archive.tar.gz", "application/gzip", "archive.tar.gz");
        verifyCreateDocumentIntent(
                new String[] {".tar.gz"}, "archive.tar.gz", "application/gzip", "archive.tar.gz");
        verifyCreateDocumentIntent(
                new String[] {},
                "archive.this_is_not_a_real_extension",
                "application/octet-stream",
                "archive.this_is_not_a_real_extension");
        verifyCreateDocumentIntent(new String[] {}, "Makefile", "*/*", "Makefile");
    }

    @Test
    public void testFileSystemAccessOpenDocumentTree() throws Exception {
        verifyFileSystemAccessIntent(Intent.ACTION_OPEN_DOCUMENT_TREE);
    }

    @Test
    public void testMimeTypesWithExternalPickerNoAcceptList() throws Exception {
        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);

        // Setup WindowAndroid#showIntent to succeed (and validate the call).
        IntentArgumentMatcher chooserIntentArgumentMatcher =
                new IntentArgumentMatcher(Intent.ACTION_CHOOSER);
        Mockito.doAnswer(
                        (invocation) -> {
                            // When showIntent is called, we use the opportunity to check on the
                            // values we expect to see within the Intent data.
                            Intent chooserIntent = (Intent) invocation.getArguments()[0];
                            Intent getContentIntent =
                                    (Intent) chooserIntent.getExtra(Intent.EXTRA_INTENT);
                            assertEquals(
                                    null, getContentIntent.getExtra(Intent.EXTRA_ALLOW_MULTIPLE));
                            assertEquals("*/*", getContentIntent.getType());
                            assertEquals(null, getContentIntent.getExtra(Intent.EXTRA_MIME_TYPES));
                            assertEquals(
                                    null, getContentIntent.getExtra(Intent.EXTRA_INITIAL_INTENTS));
                            assertTrue(getContentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
                            assertEquals(
                                    null,
                                    getContentIntent.getExtra(DocumentsContract.EXTRA_INITIAL_URI));
                            assertEquals(null, getContentIntent.getExtra(Intent.EXTRA_TITLE));
                            return true;
                        })
                .when(windowAndroid)
                .showIntent(
                        ArgumentMatchers.argThat(chooserIntentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        // Select an empty MIME type.
        selectFileDialog.selectFile(
                Intent.ACTION_GET_CONTENT,
                new String[] {},
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(0, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();
    }

    public void verifyExternalPickerWithFileExtensions(String intentAction) throws Exception {
        ShadowMimeTypeMap shadowMimeTypeMap = Shadows.shadowOf(MimeTypeMap.getSingleton());
        shadowMimeTypeMap.addExtensionMimeTypeMapping("jpg", "image/jpeg");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("gif", "image/gif");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("pdf", "application/pdf");

        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);

        // Select simple file extensions without setting up successful intent handling, to
        // simulate the pipeline aborting because showIntent fails.
        int callCount = mOnActionCallback.getCallCount();
        selectFileDialog.selectFile(
                intentAction,
                new String[] {".pdf", ".jpg"},
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);
        mOnActionCallback.waitForCallback(callCount, 1);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(1, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();

        // Now setup WindowAndroid#showIntent to succeed for our next run.
        IntentArgumentMatcher chooserIntentArgumentMatcher =
                new IntentArgumentMatcher(Intent.ACTION_CHOOSER);
        Mockito.doAnswer(
                        (invocation) -> {
                            // When showIntent is called, we use the opportunity to check on the
                            // values we expect to see within the Intent data.
                            Intent chooserIntent = (Intent) invocation.getArguments()[0];
                            Intent getContentIntent =
                                    (Intent) chooserIntent.getExtra(Intent.EXTRA_INTENT);
                            assertEquals(
                                    null, getContentIntent.getExtra(Intent.EXTRA_ALLOW_MULTIPLE));
                            assertEquals("*/*", getContentIntent.getType());
                            String[] mimeTypes =
                                    (String[]) getContentIntent.getExtra(Intent.EXTRA_MIME_TYPES);
                            assertArrayEquals(
                                    new String[] {
                                        "application/pdf",
                                        "image/jpeg",
                                        "image/gif",
                                        "type/nonexistent"
                                    },
                                    mimeTypes);
                            assertEquals(
                                    null, getContentIntent.getExtra(Intent.EXTRA_INITIAL_INTENTS));
                            assertTrue(getContentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
                            assertEquals(
                                    null,
                                    getContentIntent.getExtra(DocumentsContract.EXTRA_INITIAL_URI));
                            assertEquals(null, getContentIntent.getExtra(Intent.EXTRA_TITLE));
                            return true;
                        })
                .when(windowAndroid)
                .showIntent(
                        ArgumentMatchers.argThat(chooserIntentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        // Simulate showing the dialog, allowing a PDF and some images to be uploaded and watch the
        // pipeline remain open.
        callCount = mOnActionCallback.getCallCount();
        selectFileDialog.selectFile(
                intentAction,
                new String[] {".pdf", ".jpg", "image/gif"},
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(0, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();

        // Setup showIntent to check for invalid file extensions. Because the MIME type is
        // generic, EXTRA_MIME_TYPES filter should be dropped.
        Mockito.doAnswer(
                        (invocation) -> {
                            Intent chooserIntent = (Intent) invocation.getArguments()[0];
                            Intent getContentIntent =
                                    (Intent) chooserIntent.getExtra(Intent.EXTRA_INTENT);
                            assertEquals(
                                    true, getContentIntent.getExtra(Intent.EXTRA_ALLOW_MULTIPLE));
                            assertEquals("*/*", getContentIntent.getType());
                            assertFalse(getContentIntent.hasExtra(Intent.EXTRA_MIME_TYPES));
                            assertEquals(
                                    null, getContentIntent.getExtra(Intent.EXTRA_INITIAL_INTENTS));
                            assertTrue(getContentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
                            assertEquals(
                                    null,
                                    getContentIntent.getExtra(DocumentsContract.EXTRA_INITIAL_URI));
                            assertEquals(null, getContentIntent.getExtra(Intent.EXTRA_TITLE));
                            return true;
                        })
                .when(windowAndroid)
                .showIntent(
                        ArgumentMatchers.argThat(chooserIntentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        // Add a media file to the mix and allow multiple files.
        callCount = mOnActionCallback.getCallCount();
        selectFileDialog.selectFile(
                intentAction,
                new String[] {".this_is_not_a_real_extension", "image/gif"},
                /* capture= */ false,
                /* multiple= */ true,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(0, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();

        // Setup showIntent to check for an empty file extensions, which will be
        // ignored.
        Mockito.doAnswer(
                        (invocation) -> {
                            Intent chooserIntent = (Intent) invocation.getArguments()[0];
                            Intent getContentIntent =
                                    (Intent) chooserIntent.getExtra(Intent.EXTRA_INTENT);
                            assertEquals(
                                    true, getContentIntent.getExtra(Intent.EXTRA_ALLOW_MULTIPLE));
                            assertEquals("*/*", getContentIntent.getType());
                            String[] mimeTypes =
                                    (String[]) getContentIntent.getExtra(Intent.EXTRA_MIME_TYPES);
                            assertArrayEquals(
                                    new String[] {"image/gif", "type/nonexistent"}, mimeTypes);
                            assertEquals(
                                    null, getContentIntent.getExtra(Intent.EXTRA_INITIAL_INTENTS));
                            assertTrue(getContentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
                            assertEquals(
                                    null,
                                    getContentIntent.getExtra(DocumentsContract.EXTRA_INITIAL_URI));
                            assertEquals(null, getContentIntent.getExtra(Intent.EXTRA_TITLE));
                            return true;
                        })
                .when(windowAndroid)
                .showIntent(
                        ArgumentMatchers.argThat(chooserIntentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        // Empty file extension is ignored.
        callCount = mOnActionCallback.getCallCount();
        selectFileDialog.selectFile(
                intentAction,
                new String[] {".", "image/gif"},
                /* capture= */ false,
                /* multiple= */ true,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(0, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();
    }

    @Test
    public void testFileSelectionUserActions() throws Exception {
        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);

        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        when(windowAndroid.hasPermission(Manifest.permission.CAMERA)).thenReturn(false);

        // Start with a simple camera capture event (which should fail because the CAMERA permission
        // is denied).
        int callCount = mOnActionCallback.getCallCount();
        selectFileDialog.selectFile(
                Intent.ACTION_GET_CONTENT,
                new String[] {"image/jpeg"},
                /* capture= */ true,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);
        mOnActionCallback.waitForCallback(callCount, 1);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(1, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();

        // The CANCEL event should also fail and not result in any files being selected.
        callCount = mOnActionCallback.getCallCount();
        selectFileDialog.onPhotoPickerUserAction(
                PhotoPickerListener.PhotoPickerAction.CANCEL, new Uri[0]);
        mOnActionCallback.waitForCallback(callCount, 1);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(1, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();

        // The PHOTOS_SELECTED event without images should have the same result.
        callCount = mOnActionCallback.getCallCount();
        selectFileDialog.onPhotoPickerUserAction(
                PhotoPickerListener.PhotoPickerAction.PHOTOS_SELECTED, new Uri[0]);
        mOnActionCallback.waitForCallback(callCount, 1);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(1, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();

        // Test LAUNCH_CAMERA, which requires a bit of mocking to make sure the permissions are
        // setup correctly (ensure that the requests for the CAMERA permission are denied).
        Mockito.doAnswer(
                        (invocation) -> {
                            PermissionCallback callback =
                                    (PermissionCallback) invocation.getArguments()[1];
                            callback.onRequestPermissionsResult(
                                    new String[] {Manifest.permission.CAMERA},
                                    new int[] {PackageManager.PERMISSION_DENIED});
                            return null;
                        })
                .when(windowAndroid)
                .requestPermissions(
                        aryEq(new String[] {Manifest.permission.CAMERA}),
                        (PermissionCallback) any());

        // Test LAUNCH_CAMERA when permission is denied. Note: this is different from the other
        // events because the MediaPicker dialog stays open and the pipeline should not shut down
        // (so onFileNotSelected should not be called). See https://crbug.com/1381455 for details.
        callCount = mOnActionCallback.getCallCount();
        selectFileDialog.onPhotoPickerUserAction(
                PhotoPickerListener.PhotoPickerAction.LAUNCH_CAMERA, new Uri[0]);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(0, selectFileDialog.mFileSelectionAborted);
        assertEquals(callCount, mOnActionCallback.getCallCount());
        selectFileDialog.resetFileSelectionAttempts();

        // Setup for another LAUNCH_CAMERA test, this time with the CAMERA permission enabled.
        Mockito.doAnswer(
                        (invocation) -> {
                            PermissionCallback callback =
                                    (PermissionCallback) invocation.getArguments()[1];
                            callback.onRequestPermissionsResult(
                                    new String[] {Manifest.permission.CAMERA},
                                    new int[] {PackageManager.PERMISSION_GRANTED});
                            return null;
                        })
                .when(windowAndroid)
                .requestPermissions(
                        aryEq(new String[] {Manifest.permission.CAMERA}),
                        (PermissionCallback) any());

        // Since the permission is now allowed, the LAUNCH_CAMERA event should keep the pipeline
        // open.
        callCount = mOnActionCallback.getCallCount();
        selectFileDialog.onPhotoPickerUserAction(
                PhotoPickerListener.PhotoPickerAction.LAUNCH_CAMERA, new Uri[0]);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(0, selectFileDialog.mFileSelectionAborted);
        assertEquals(callCount, mOnActionCallback.getCallCount());
        selectFileDialog.resetFileSelectionAttempts();

        // Test the LAUNCH_GALLERY event (which normally opens the Files app). However, by default
        // the showIntent will fail on the mock WindowAndroid object, so the file selection should
        // be aborted.
        selectFileDialog.setFileTypesForTests(new ArrayList<>(Arrays.asList("image/jpeg")));

        callCount = mOnActionCallback.getCallCount();
        selectFileDialog.onPhotoPickerUserAction(
                PhotoPickerListener.PhotoPickerAction.LAUNCH_GALLERY, new Uri[0]);
        mOnActionCallback.waitForCallback(callCount, 1);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(1, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();

        // Force WindowAndroid#showIntent to succeed and make sure the pipeline remains open when
        // the test reruns.
        IntentArgumentMatcher chooserIntentArgumentMatcher =
                new IntentArgumentMatcher(Intent.ACTION_CHOOSER);
        Mockito.doAnswer(
                        (invocation) -> {
                            Intent chooserIntent = (Intent) invocation.getArguments()[0];
                            Intent getContentIntent =
                                    (Intent) chooserIntent.getExtra(Intent.EXTRA_INTENT);
                            assertEquals(
                                    null, getContentIntent.getExtra(Intent.EXTRA_ALLOW_MULTIPLE));
                            assertEquals("*/*", getContentIntent.getType());
                            String[] mimeTypes =
                                    (String[]) getContentIntent.getExtra(Intent.EXTRA_MIME_TYPES);
                            assertArrayEquals(
                                    new String[] {"image/jpeg", "type/nonexistent"}, mimeTypes);
                            assertEquals(
                                    null, getContentIntent.getExtra(Intent.EXTRA_INITIAL_INTENTS));
                            assertEquals(
                                    null,
                                    getContentIntent.getExtra(DocumentsContract.EXTRA_INITIAL_URI));
                            assertEquals(null, getContentIntent.getExtra(Intent.EXTRA_TITLE));
                            return true;
                        })
                .when(windowAndroid)
                .showIntent(
                        ArgumentMatchers.argThat(chooserIntentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        // Rerun the test. Because showIntent now reports success, the upload should still be in
        // progress.
        callCount = mOnActionCallback.getCallCount();
        selectFileDialog.onPhotoPickerUserAction(
                PhotoPickerListener.PhotoPickerAction.LAUNCH_GALLERY, new Uri[0]);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(0, selectFileDialog.mFileSelectionAborted);
        assertEquals(callCount, mOnActionCallback.getCallCount());
        selectFileDialog.resetFileSelectionAttempts();
    }

    @Test
    public void testFileSelectionPermissionInterrupted() throws Exception {
        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);

        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        when(windowAndroid.hasPermission(Manifest.permission.CAMERA)).thenReturn(false);

        IntentArgumentMatcher imageCaptureIntentArgumentMatcher =
                new IntentArgumentMatcher(MediaStore.ACTION_IMAGE_CAPTURE);
        when(windowAndroid.canResolveActivity(
                        ArgumentMatchers.argThat(imageCaptureIntentArgumentMatcher)))
                .thenReturn(true);

        // Setup the request callback to simulate an interrupted permission flow.
        Mockito.doAnswer(
                        (invocation) -> {
                            PermissionCallback callback =
                                    (PermissionCallback) invocation.getArguments()[1];
                            callback.onRequestPermissionsResult(new String[] {}, new int[] {});
                            return null;
                        })
                .when(windowAndroid)
                .requestPermissions(
                        aryEq(new String[] {Manifest.permission.CAMERA}),
                        (PermissionCallback) any());

        // Ensure permission request in selectFile can handle interrupted permission flow.
        int callCount = mOnActionCallback.getCallCount();
        selectFileDialog.selectFile(
                Intent.ACTION_GET_CONTENT,
                new String[] {"image/jpeg"},
                /* capture= */ true,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);
        mOnActionCallback.waitForCallback(callCount, 1);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(1, selectFileDialog.mFileSelectionAborted);
        selectFileDialog.resetFileSelectionAttempts();

        // Ensure permission request in onPhotoPickerUserAction can handle interrupted permission
        // flow.
        callCount = mOnActionCallback.getCallCount();
        selectFileDialog.onPhotoPickerUserAction(
                PhotoPickerListener.PhotoPickerAction.LAUNCH_CAMERA, new Uri[0]);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(0, selectFileDialog.mFileSelectionAborted);
        assertEquals(callCount, mOnActionCallback.getCallCount());
        selectFileDialog.resetFileSelectionAttempts();
    }

    @Test
    public void testVideoCaptureRequestPermissionSuccess() throws Exception {
        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);

        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        when(windowAndroid.hasPermission(Manifest.permission.CAMERA))
                .thenReturn(false)
                .thenReturn(true);

        IntentArgumentMatcher videoCaptureIntentArgumentMatcher =
                new IntentArgumentMatcher(MediaStore.ACTION_VIDEO_CAPTURE);
        when(windowAndroid.canResolveActivity(
                        ArgumentMatchers.argThat(videoCaptureIntentArgumentMatcher)))
                .thenReturn(true);

        // Setup the request callback to simulate an interrupted permission flow.
        Mockito.doAnswer(
                        (invocation) -> {
                            PermissionCallback callback =
                                    (PermissionCallback) invocation.getArguments()[1];
                            callback.onRequestPermissionsResult(
                                    new String[] {Manifest.permission.CAMERA},
                                    new int[] {PackageManager.PERMISSION_GRANTED});
                            return null;
                        })
                .when(windowAndroid)
                .requestPermissions(
                        aryEq(new String[] {Manifest.permission.CAMERA}),
                        (PermissionCallback) any());

        AtomicBoolean cameraIntentShow = new AtomicBoolean(false);
        IntentArgumentMatcher chooserIntentArgumentMatcher =
                new IntentArgumentMatcher(MediaStore.ACTION_VIDEO_CAPTURE);
        Mockito.doAnswer(
                        (invocation) -> {
                            cameraIntentShow.set(true);
                            return true;
                        })
                .when(windowAndroid)
                .showIntent(
                        ArgumentMatchers.argThat(chooserIntentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        // Ensure permission request in selectFile can handle interrupted permission flow.
        selectFileDialog.selectFile(
                Intent.ACTION_GET_CONTENT,
                new String[] {"video/*"},
                /* capture= */ true,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);

        assertTrue(cameraIntentShow.get());
        selectFileDialog.resetFileSelectionAttempts();
    }

    /** Returns the determined scope for the accepted |fileTypes|. */
    private int scopeForFileTypes(String... fileTypes) {
        SelectFileDialog instance = SelectFileDialog.create(/* nativeSelectFileDialog= */ null);
        instance.setFileTypesForTests(new ArrayList<>(Arrays.asList(fileTypes)));

        return instance.determineSelectFileDialogScope();
    }

    @Test
    public void testDetermineSelectFileDialogScope() {
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC, scopeForFileTypes());
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC, scopeForFileTypes("*/*"));
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC, scopeForFileTypes("text/plain"));

        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES, scopeForFileTypes("image/*"));
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES, scopeForFileTypes("image/png"));
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes("image/*", "test/plain"));

        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_VIDEOS, scopeForFileTypes("video/*"));
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_VIDEOS, scopeForFileTypes("video/ogg"));
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes("video/*", "test/plain"));
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES,
                scopeForFileTypes("image/x-png", "image/gif", "image/jpeg"));
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes("image/x-png", "image/gif", "image/jpeg", "text/plain"));

        // Test image extensions only.
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES,
                scopeForFileTypes(".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp"));
        // Test image extensions mixed with image MIME types.
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES,
                scopeForFileTypes(".JPG", ".jpeg", "image/gif", "image/jpeg"));
        // Image extensions mixed with image MIME types and other.
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes(".jpg", "image/gif", "text/plain"));
        // Video extensions only.
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_VIDEOS,
                scopeForFileTypes(".avi", ".mov", ".mp4", ".mpeg", ".webm", ".mkv"));
        // Video extensions and video MIME types.
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_VIDEOS,
                scopeForFileTypes(".avi", ".mp4", "video/ogg"));
        // Video extensions and video MIME types and other.
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes(".avi", ".mp4", "video/ogg", "text/plain"));

        // Audio + video should result in generic.
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes(".avi", "video/ogg", "audio/mp4", ".mp3"));

        // Non-image, non-video extension only.
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC, scopeForFileTypes(".doc"));

        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES_AND_VIDEOS,
                scopeForFileTypes("video/*", "image/*"));
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES_AND_VIDEOS,
                scopeForFileTypes("image/jpeg", "video/ogg"));
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes("video/*", "image/*", "text/plain"));
    }

    @Test
    public void testPhotoPickerLaunchAndMimeTypes() {
        ShadowMimeTypeMap shadowMimeTypeMap = Shadows.shadowOf(MimeTypeMap.getSingleton());
        shadowMimeTypeMap.addExtensionMimeTypeMapping("jpg", "image/jpeg");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("gif", "image/gif");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("txt", "text/plain");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("mpg", "video/mpeg");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("gz", "application/gzip");

        assertEquals("", SelectFileDialog.ensureMimeType(""));
        assertEquals("image/jpeg", SelectFileDialog.ensureMimeType(".jpg"));
        assertEquals("image/jpeg", SelectFileDialog.ensureMimeType("image/jpeg"));
        assertEquals("application/gzip", SelectFileDialog.ensureMimeType(".tar.gz"));
        // Unknown extension, expect default response:
        assertEquals(
                "application/octet-stream",
                SelectFileDialog.ensureMimeType(".this_is_not_a_real_extension"));

        assertFalse(SelectFileDialog.isSupportedPhotoPickerTypes(new ArrayList<>()));
        assertFalse(SelectFileDialog.isSupportedPhotoPickerTypes(Arrays.asList("")));
        assertFalse(SelectFileDialog.isSupportedPhotoPickerTypes(Arrays.asList("foo/bar")));
        // ".jpg" is not a valid MIME type.
        assertFalse(SelectFileDialog.isSupportedPhotoPickerTypes(Arrays.asList(".jpg")));
        assertTrue(SelectFileDialog.isSupportedPhotoPickerTypes(Arrays.asList("image/jpeg")));
        // If one of the file type is not a valid MIME type, the method should return false.
        assertFalse(
                SelectFileDialog.isSupportedPhotoPickerTypes(Arrays.asList(".jpg", "image/jpeg")));
        assertFalse(
                SelectFileDialog.isSupportedPhotoPickerTypes(Arrays.asList(".gif", "image/jpeg")));
        assertTrue(
                SelectFileDialog.isSupportedPhotoPickerTypes(
                        Arrays.asList("image/gif", "image/jpeg")));

        // Video and mixed video/images support. This feature is supported, but off by default, so
        // expect failure until it is turned on by default.
        assertFalse(SelectFileDialog.isSupportedPhotoPickerTypes(Arrays.asList(".mpg")));
        assertFalse(SelectFileDialog.isSupportedPhotoPickerTypes(Arrays.asList("video/mpeg")));
        assertFalse(
                SelectFileDialog.isSupportedPhotoPickerTypes(
                        Arrays.asList("image/jpeg", "video/mpeg")));
        assertFalse(
                SelectFileDialog.isSupportedPhotoPickerTypes(
                        Arrays.asList(".jpg", "image/jpeg", ".mpg")));

        // Returns null because generic picker is required (due to addition of .txt file).
        assertFalse(
                SelectFileDialog.isSupportedPhotoPickerTypes(
                        Arrays.asList(".txt", ".jpg", "image/jpeg")));
    }

    @Test
    public void testMultipleFileSelectorWithFileUris() throws IOException {
        SelectFileDialog selectFileDialog = new SelectFileDialog(null);
        File file0 = mTempFolder.newFile("IMG_0.jpg");
        File file1 = mTempFolder.newFile("IMG_1.jpg");
        Uri[] filePathArray =
                new Uri[] {
                    Uri.parse("file://" + file0.getAbsolutePath()),
                    Uri.parse("file://" + file1.getAbsolutePath())
                };
        SelectFileDialog.GetDisplayNameTask task =
                selectFileDialog
                .new GetDisplayNameTask(ContextUtils.getApplicationContext(), true, filePathArray);
        task.doInBackground();
        assertEquals(file0.getCanonicalPath(), task.mFilePaths[0]);
        assertEquals(file1.getCanonicalPath(), task.mFilePaths[1]);
    }

    @Test
    public void testMultipleFileSelectorWithCaseVariantFileUris() throws IOException {
        SelectFileDialog selectFileDialog = new SelectFileDialog(null);
        File file0 = mTempFolder.newFile("IMG_0.jpg");
        File file1 = mTempFolder.newFile("IMG_1.jpg");
        Uri[] filePathArray =
                new Uri[] {
                    Uri.parse("FILE://" + file0.getAbsolutePath()),
                    Uri.parse("File://" + file1.getAbsolutePath())
                };
        SelectFileDialog.GetDisplayNameTask task =
                selectFileDialog
                .new GetDisplayNameTask(ContextUtils.getApplicationContext(), true, filePathArray);
        task.doInBackground();
        assertEquals(file0.getCanonicalPath(), task.mFilePaths[0]);
        assertEquals(file1.getCanonicalPath(), task.mFilePaths[1]);
    }

    @Test
    public void testMultipleFileSelectorWithCaseVariantAppDirUris() throws IOException {
        SelectFileDialog selectFileDialog = new SelectFileDialog(null);
        File dataDir = ContextCompat.getDataDir(ContextUtils.getApplicationContext());
        File appFile = new File(dataDir, "app_data.txt");
        dataDir.mkdirs();
        assertTrue(appFile.createNewFile());
        Uri[] filePathArray = new Uri[] {Uri.parse("FILE://" + appFile.getAbsolutePath())};
        SelectFileDialog.GetDisplayNameTask task =
                selectFileDialog
                .new GetDisplayNameTask(ContextUtils.getApplicationContext(), true, filePathArray);
        assertEquals(null, task.doInBackground());
    }

    @Test
    @DisableFeatures({UiAndroidFeatures.CHECK_INTENT_CALLER_PERMISSION})
    public void testIntentCompletedWithCaseVariantClipDataUri() throws IOException {
        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);
        File file = mTempFolder.newFile("IMG_0.jpg");
        ClipData clipData =
                ClipData.newUri(
                        ContextUtils.getApplicationContext().getContentResolver(),
                        "label",
                        Uri.parse("FILE://" + file.getAbsolutePath()));
        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setClipData(clipData);
        selectFileDialog.onIntentCompleted(Activity.RESULT_OK, intent);
        runAllAsyncTasks();
        assertEquals(1, selectFileDialog.mFileSelectionSuccess);
        assertEquals(0, selectFileDialog.mFileSelectionAborted);
    }

    @Test
    @DisableFeatures({UiAndroidFeatures.CHECK_INTENT_CALLER_PERMISSION})
    public void testIntentCompletedWithCaseVariantDataUri() {
        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("CONTENT://com.example.test/xyz"));
        selectFileDialog.onIntentCompleted(Activity.RESULT_OK, intent);
        runAllAsyncTasks();
        assertEquals(1, selectFileDialog.mFileSelectionSuccess);
        assertEquals(0, selectFileDialog.mFileSelectionAborted);
    }

    @Test
    public void testMultipleFileSelectorWithSchemelessUris() {
        SelectFileDialog selectFileDialog = new SelectFileDialog(null);
        Uri[] filePathArray =
                new Uri[] {Uri.parse("/data/data/com.android.chrome/app_chrome/Default/Cookies")};
        SelectFileDialog.GetDisplayNameTask task =
                selectFileDialog
                .new GetDisplayNameTask(ContextUtils.getApplicationContext(), true, filePathArray);
        assertEquals(null, task.doInBackground());
    }

    @Test
    public void testMultipleFileSelectorWithInvalidSchemeUris() {
        SelectFileDialog selectFileDialog = new SelectFileDialog(null);
        Uri[] filePathArray = new Uri[] {Uri.parse("http://example.com/test.jpg")};
        SelectFileDialog.GetDisplayNameTask task =
                selectFileDialog
                .new GetDisplayNameTask(ContextUtils.getApplicationContext(), true, filePathArray);
        assertEquals(null, task.doInBackground());
    }

    private void testFilePath(
            String path, SelectFileDialog selectFileDialog, boolean expectedPass) {
        Uri[] uris = new Uri[1];
        uris[0] = Uri.fromFile(new File(path));

        SelectFileDialog.FilePathSelectedTask task =
                selectFileDialog
                .new FilePathSelectedTask(ContextUtils.getApplicationContext(), path, null);
        SelectFileDialog.GetDisplayNameTask task2 =
                selectFileDialog
                .new GetDisplayNameTask(
                        ContextUtils.getApplicationContext(), /* isMultiple= */ false, uris);
        assertEquals(expectedPass, task.doInBackground());
        assertEquals(expectedPass, null != task2.doInBackground());
    }

    @Test
    public void testFilePathTasks() throws IOException {
        SelectFileDialog selectFileDialog = new SelectFileDialog(null);

        // Obtain the data directory for RoboElectric. It should look something like:
        //   /tmp/robolectric-Method_[testName][number]/org.chromium.test.ui-dataDir
        // ... where [testName] is the name of this test function and [number] is a unique id.
        String dataDir =
                ContextCompat.getDataDir(ContextUtils.getApplicationContext()).getCanonicalPath();
        assertTrue(new File(dataDir, "tmp").mkdirs());
        assertTrue(new File(dataDir, "tmp/xyz.jpg").createNewFile());
        assertTrue(new File(dataDir, "xyz.jpg").createNewFile());
        assertTrue(new File(dataDir, "../xyz.jpg").createNewFile());

        // Passing in the data directory itself should fail.
        testFilePath(dataDir, selectFileDialog, /* expectedPass= */ false);
        // Passing in a subdirectory of the data directory should also fail.
        testFilePath(dataDir + "/tmp/xyz.jpg", selectFileDialog, /* expectedPass= */ false);
        // The parent directory of the data directory should, however, succeed.
        testFilePath(dataDir + "/../xyz.jpg", selectFileDialog, /* expectedPass= */ true);
        // Another way of specifying the data directory (should fail).
        testFilePath(dataDir + "/tmp/../xyz.jpg", selectFileDialog, /* expectedPass= */ false);
        // The directory outside the data directory should succeed.
        testFilePath(
                mTempFolder.newFile("tmp.jpg").getPath(),
                selectFileDialog,
                /* expectedPass= */ true);

        Path path = new File(dataDir).toPath();
        String parent = path.getParent().toString();
        String lastComponent = path.getName(path.getNameCount() - 1).toString();

        // Make sure that base/./dataDir is treated the same as base/dataDir (and fail the request).
        testFilePath(
                parent + "/./" + lastComponent + "/xyz.jpg",
                selectFileDialog,
                /* expectedPass= */ false);
        // Make sure that dataDir/../dataDir is treated the same as dataDir (and fail the request).
        testFilePath(
                dataDir + "/../" + lastComponent + "/xyz.jpg",
                selectFileDialog,
                /* expectedPass= */ false);

        // Invalid or nonexistent file paths should fail.
        testFilePath("\\/tmp/xyz.jpg", selectFileDialog, /* expectedPass= */ false);
        testFilePath(
                mTempFolder.getRoot() + "/nonexistent.jpg",
                selectFileDialog,
                /* expectedPass= */ false);
    }

    @Test
    public void testFilePathTasksRejectProcAndNonRegularFiles() {
        SelectFileDialog selectFileDialog = new SelectFileDialog(null);

        // Paths resolving into procfs (here /proc/<pid>/maps) are rejected.
        testFilePath("/proc/self/maps", selectFileDialog, /* expectedPass= */ false);
        // Directories are not regular files.
        testFilePath(mTempFolder.getRoot().getPath(), selectFileDialog, /* expectedPass= */ false);
    }

    @Test
    public void testFilePathTasksForwardResolvedPath() throws IOException {
        SelectFileDialog selectFileDialog = new SelectFileDialog(null);
        File target = mTempFolder.newFile("target.jpg");
        File link = new File(mTempFolder.getRoot(), "link.jpg");
        Files.createSymbolicLink(link.toPath(), target.toPath());

        SelectFileDialog.GetDisplayNameTask task =
                selectFileDialog
                .new GetDisplayNameTask(
                        ContextUtils.getApplicationContext(),
                        /* isMultiple= */ false,
                        new Uri[] {Uri.fromFile(link)});
        task.doInBackground();
        assertEquals(target.getCanonicalPath(), task.mFilePaths[0]);
    }

    @Test
    @DisableFeatures({UiAndroidFeatures.CHECK_INTENT_CALLER_PERMISSION})
    public void testFileSchemeRejectsOpaqueAndRelativeUris() throws IOException {
        File validFile = mTempFolder.newFile("valid.jpg");

        // Even if realpath() were to resolve a non-hierarchical or relative path to an existing
        // regular file, opaque file: URIs (such as "file:content://...") and relative paths must
        // be rejected before reaching FileUtils.getAbsoluteFilePath().
        Mockito.lenient()
                .doReturn(validFile.getCanonicalPath())
                .when(mFileUtilsMocks)
                .getAbsoluteFilePath(any());

        SelectFileDialog selectFileDialog = new SelectFileDialog(null);
        Uri[] opaqueAndRelativeUris =
                new Uri[] {
                    Uri.parse("file:content://com.android.chrome.FileProvider/images/pic.jpg"),
                    Uri.parse("FILE:content://com.android.chrome.FileProvider/images/pic.jpg"),
                    Uri.parse("file:valid.jpg"),
                    Uri.fromParts("file", "valid.jpg", null),
                };
        for (Uri uri : opaqueAndRelativeUris) {
            SelectFileDialog.GetDisplayNameTask task =
                    selectFileDialog
                    .new GetDisplayNameTask(
                            ContextUtils.getApplicationContext(),
                            /* isMultiple= */ false,
                            new Uri[] {uri});
            assertEquals("Expected null for " + uri, null, task.doInBackground());
        }

        SelectFileDialog.FilePathSelectedTask relativePathTask =
                selectFileDialog
                .new FilePathSelectedTask(ContextUtils.getApplicationContext(), "valid.jpg", null);
        assertFalse(relativePathTask.doInBackground());

        // End-to-end onIntentCompleted checks with opaque file:content:// URI in ClipData and data.
        TestSelectFileDialog testDialog = new TestSelectFileDialog(null);
        Uri opaqueContentUri =
                Uri.parse("file:content://com.android.chrome.FileProvider/images/pic.jpg");
        Intent clipIntent = new Intent();
        clipIntent.setClipData(
                new ClipData("label", new String[0], new ClipData.Item(opaqueContentUri)));
        testDialog.onIntentCompleted(Activity.RESULT_OK, clipIntent);
        runAllAsyncTasks();
        assertEquals(0, testDialog.mFileSelectionSuccess);
        assertEquals(1, testDialog.mFileSelectionAborted);
        testDialog.resetFileSelectionAttempts();

        Intent dataIntent = new Intent(Intent.ACTION_VIEW, opaqueContentUri);
        testDialog.onIntentCompleted(Activity.RESULT_OK, dataIntent);
        runAllAsyncTasks();
        assertEquals(0, testDialog.mFileSelectionSuccess);
        assertEquals(1, testDialog.mFileSelectionAborted);
        testDialog.resetFileSelectionAttempts();
        Mockito.verify(mFileUtilsMocks, Mockito.never()).getAbsoluteFilePath(any());

        // Valid hierarchical file:/// URI still succeeds.
        SelectFileDialog.GetDisplayNameTask validTask =
                selectFileDialog
                .new GetDisplayNameTask(
                        ContextUtils.getApplicationContext(),
                        /* isMultiple= */ false,
                        new Uri[] {Uri.fromFile(validFile)});
        assertTrue(validTask.doInBackground() != null);
        Mockito.verify(mFileUtilsMocks).getAbsoluteFilePath(validFile.getPath());
        assertEquals(validFile.getCanonicalPath(), validTask.mFilePaths[0]);
    }

    @Test
    public void testVirtualDocumentPathResultsAreRejected() throws IOException {
        File validFile = mTempFolder.newFile("doc.pdf");
        Mockito.lenient()
                .doReturn(validFile.getCanonicalPath())
                .when(mFileUtilsMocks)
                .getAbsoluteFilePath(any());

        SelectFileDialog selectFileDialog = new SelectFileDialog(null);
        Uri[] virtualPathUris =
                new Uri[] {
                    Uri.parse("file:/SAF/authority/tree/123/doc.pdf"),
                    Uri.parse("file:///SAF/authority/tree/123/note.txt"),
                };
        for (Uri uri : virtualPathUris) {
            SelectFileDialog.GetDisplayNameTask task =
                    selectFileDialog
                    .new GetDisplayNameTask(
                            ContextUtils.getApplicationContext(),
                            /* isMultiple= */ false,
                            new Uri[] {uri});
            assertNull("Expected null for " + uri, task.doInBackground());
        }
        SelectFileDialog.GetDisplayNameTask multiTask =
                selectFileDialog
                .new GetDisplayNameTask(
                        ContextUtils.getApplicationContext(),
                        /* isMultiple= */ true,
                        virtualPathUris);
        assertNull(multiTask.doInBackground());

        String[] virtualPaths = new String[] {"/SAF", "/SAF/", "/SAF/authority/tree/123/doc.pdf"};
        for (String path : virtualPaths) {
            SelectFileDialog.FilePathSelectedTask task =
                    selectFileDialog
                    .new FilePathSelectedTask(ContextUtils.getApplicationContext(), path, null);
            assertFalse("Expected false for " + path, task.doInBackground());
        }

        TestSelectFileDialog testDialog = new TestSelectFileDialog(null);
        ClipData clipData =
                ClipData.newUri(
                        ContextUtils.getApplicationContext().getContentResolver(),
                        "label",
                        virtualPathUris[0]);
        clipData.addItem(new ClipData.Item(virtualPathUris[1]));
        Intent clipIntent = new Intent(Intent.ACTION_VIEW);
        clipIntent.setClipData(clipData);
        testDialog.onIntentCompleted(Activity.RESULT_OK, clipIntent);
        runAllAsyncTasks();
        assertEquals(0, testDialog.mFileSelectionSuccess);
        assertEquals(1, testDialog.mFileSelectionAborted);
        testDialog.resetFileSelectionAttempts();

        Intent dataIntent = new Intent(Intent.ACTION_VIEW, virtualPathUris[0]);
        testDialog.onIntentCompleted(Activity.RESULT_OK, dataIntent);
        runAllAsyncTasks();
        assertEquals(0, testDialog.mFileSelectionSuccess);
        assertEquals(1, testDialog.mFileSelectionAborted);
        testDialog.resetFileSelectionAttempts();

        Mockito.verify(mFileUtilsMocks, Mockito.never()).getAbsoluteFilePath(any());

        SelectFileDialog.GetDisplayNameTask mixedMultiTask =
                selectFileDialog
                .new GetDisplayNameTask(
                        ContextUtils.getApplicationContext(),
                        /* isMultiple= */ true,
                        new Uri[] {Uri.fromFile(validFile), virtualPathUris[0]});
        assertNull(mixedMultiTask.doInBackground());

        doReturn(validFile.getCanonicalPath())
                .when(mFileUtilsMocks)
                .getAbsoluteFilePath("/SAFE/doc.pdf");
        SelectFileDialog.FilePathSelectedTask safePrefixTask =
                selectFileDialog
                .new FilePathSelectedTask(
                        ContextUtils.getApplicationContext(), "/SAFE/doc.pdf", null);
        assertTrue(safePrefixTask.doInBackground());
        Mockito.verify(mFileUtilsMocks).getAbsoluteFilePath("/SAFE/doc.pdf");

        doReturn("/SAF/authority/tree/123/doc.pdf")
                .when(mFileUtilsMocks)
                .getAbsoluteFilePath("//SAF/doc.pdf");
        SelectFileDialog.FilePathSelectedTask postResolveFileTask =
                selectFileDialog
                .new FilePathSelectedTask(
                        ContextUtils.getApplicationContext(), "//SAF/doc.pdf", null);
        assertFalse(postResolveFileTask.doInBackground());
        SelectFileDialog.GetDisplayNameTask postResolveUriTask =
                selectFileDialog
                .new GetDisplayNameTask(
                        ContextUtils.getApplicationContext(),
                        /* isMultiple= */ false,
                        new Uri[] {Uri.parse("file:////SAF/doc.pdf")});
        assertNull(postResolveUriTask.doInBackground());
        Mockito.verify(mFileUtilsMocks, Mockito.times(2)).getAbsoluteFilePath("//SAF/doc.pdf");
    }

    @Test
    @DisableFeatures({UiAndroidFeatures.CHECK_INTENT_CALLER_PERMISSION})
    public void testContentSchemeRejectsOpaqueAndSameAppUris() {
        Context context = ContextUtils.getApplicationContext();
        String pkg = context.getPackageName();
        String sameAppAuthority = pkg + ".TestProvider";
        ShadowPackageManager shadowPm = Shadows.shadowOf(context.getPackageManager());
        shadowPm.getInternalMutablePackageInfo(pkg).applicationInfo.uid = Process.myUid();
        ProviderInfo providerInfo = new ProviderInfo();
        providerInfo.authority = sameAppAuthority;
        providerInfo.packageName = pkg;
        providerInfo.name = "TestProvider";
        providerInfo.applicationInfo = shadowPm.getInternalMutablePackageInfo(pkg).applicationInfo;
        shadowPm.addOrUpdateProvider(providerInfo);

        Uri[] rejectedUris =
                new Uri[] {
                    new Uri.Builder()
                            .scheme("content")
                            .encodedOpaquePart("//com.example.app.provider/files/doc.txt")
                            .build(),
                    new Uri.Builder()
                            .scheme("CONTENT")
                            .encodedOpaquePart("//com.example.app.provider/files/doc.txt")
                            .build(),
                    new Uri.Builder()
                            .scheme("content")
                            .encodedPath("//com.example.external/doc.txt")
                            .build(),
                    Uri.parse("content:///files/doc.txt"),
                    Uri.parse("content://" + sameAppAuthority + "/files/missing.txt"),
                };

        TestSelectFileDialog testDialog = new TestSelectFileDialog(null);
        for (Uri uri : rejectedUris) {
            Intent dataIntent = new Intent(Intent.ACTION_VIEW, uri);
            testDialog.onIntentCompleted(Activity.RESULT_OK, dataIntent);
            runAllAsyncTasks();
            assertEquals("Expected 0 successes for " + uri, 0, testDialog.mFileSelectionSuccess);
            assertEquals("Expected 1 abort for " + uri, 1, testDialog.mFileSelectionAborted);
            testDialog.resetFileSelectionAttempts();

            Intent clipIntent = new Intent();
            clipIntent.setClipData(new ClipData("label", new String[0], new ClipData.Item(uri)));
            testDialog.onIntentCompleted(Activity.RESULT_OK, clipIntent);
            runAllAsyncTasks();
            assertEquals("Expected 0 successes for " + uri, 0, testDialog.mFileSelectionSuccess);
            assertEquals("Expected 1 abort for " + uri, 1, testDialog.mFileSelectionAborted);
            testDialog.resetFileSelectionAttempts();
        }

        Uri externalUri = Uri.parse("content://com.example.external/doc.txt");
        testDialog.onIntentCompleted(
                Activity.RESULT_OK, new Intent(Intent.ACTION_VIEW, externalUri));
        runAllAsyncTasks();
        assertEquals(1, testDialog.mFileSelectionSuccess);
        assertEquals(0, testDialog.mFileSelectionAborted);
    }

    @Test
    public void testShowTypes() {
        ShadowMimeTypeMap shadowMimeTypeMap = Shadows.shadowOf(MimeTypeMap.getSingleton());
        shadowMimeTypeMap.addExtensionMimeTypeMapping("jpg", "image/jpeg");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("gif", "image/gif");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("pdf", "application/pdf");

        SelectFileDialog selectFileDialog = new SelectFileDialog(null);

        selectFileDialog.setFileTypesForTests(Arrays.asList("image/jpeg"));
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("image/jpeg", "image/png"));
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("image/*", "image/jpeg"));
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("image/*", "video/mp4"));
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("image/jpeg", "video/mp4"));
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("video/mp4"));
        assertFalse(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("video/mp4", "video/*"));
        assertFalse(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("audio/wave", "audio/mpeg", "audio/*"));
        assertFalse(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertTrue(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("audio/wave", "audio/mpeg"));
        assertFalse(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertTrue(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("*/*"));
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertTrue(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Collections.emptyList());
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertTrue(selectFileDialog.shouldShowAudioTypes());

        // Only the first one is considered valid MIME type.
        selectFileDialog.setFileTypesForTests(Arrays.asList("image//png", "image/", "image"));
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        // Both are invalid MIME types
        selectFileDialog.setFileTypesForTests(Arrays.asList("/image", "/"));
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertTrue(selectFileDialog.shouldShowAudioTypes());

        // Same, both are invalid MIME types
        selectFileDialog.setFileTypesForTests(Arrays.asList("/", ""));
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertTrue(selectFileDialog.shouldShowAudioTypes());

        // Invalid extension
        selectFileDialog.setFileTypesForTests(Arrays.asList(".this_is_not_a_real_extension"));
        assertFalse(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        // Both are converted to the same MIME type
        selectFileDialog.setFileTypesForTests(
                Arrays.asList("application/octet-stream", ".this_is_not_a_real_extension"));
        assertFalse(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(
                Arrays.asList(".pdf", ".this_is_not_a_real_extension"));
        assertFalse(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList(".gif", "image/gif"));
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList(".gif", ".jpg"));
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());
    }

    @Test
    public void testConvertToSupportedMimeTypes() {
        // We use standard extensions here because ShadowMimeTypeMap.addExtensionMimeTypeMapping is
        // ignored on SDK 36 and above.
        assertEquals(
                SelectFileDialog.convertToSupportedMimeTypes(Arrays.asList("image/jpeg", ".jpg")),
                Arrays.asList("image/jpeg"));

        // No dot means it is not recognized as an extension.
        assertEquals(
                SelectFileDialog.convertToSupportedMimeTypes(Arrays.asList("image/gif", "jpg")),
                Arrays.asList("image/gif"));

        // An extension that is definitely not known by the platform.
        assertEquals(
                SelectFileDialog.convertToSupportedMimeTypes(
                        Arrays.asList("image/gif", ".this_is_not_a_real_extension")),
                Arrays.asList("image/gif", "application/octet-stream"));

        // Empty extensions are omitted.
        assertEquals(
                SelectFileDialog.convertToSupportedMimeTypes(Arrays.asList("image/gif", ".", "")),
                Arrays.asList("image/gif"));

        // Compound extension (.tar.gz) resolves to application/gzip.
        assertEquals(
                SelectFileDialog.convertToSupportedMimeTypes(Arrays.asList(".tar.gz")),
                Arrays.asList("application/gzip"));
    }

    ContentResolver getMockContentResolver(String mimeType) {
        final ContentResolver contentResolver = Mockito.mock(ContentResolver.class);
        final Cursor cursor = Mockito.mock(Cursor.class);

        String[] filePathColumn = {
            MediaStore.Files.FileColumns.MIME_TYPE,
        };
        if ("THROW".equals(mimeType)) {
            Mockito.doThrow(new RuntimeException())
                    .when(contentResolver)
                    .query(any(), eq(filePathColumn), any(), any(), any());
        } else {
            Mockito.doReturn(cursor)
                    .when(contentResolver)
                    .query(any(), eq(filePathColumn), any(), any(), any());
            Mockito.doReturn(true).when(cursor).moveToFirst();
            Mockito.doReturn(true).when(cursor).moveToNext();
            Mockito.doReturn(0).when(cursor).getColumnIndex(MediaStore.Files.FileColumns.MIME_TYPE);
            Mockito.doReturn(mimeType).when(cursor).getString(0);
        }
        return contentResolver;
    }

    HistogramWatcher getHistogramWatcher(
            @SelectFileDialog.FileSelectedAction int expectAction,
            @SelectFileDialog.FileSelectedUploadMethod int expectMethod) {
        return HistogramWatcher.newBuilder()
                .expectIntRecord("Android.SelectFileDialogContentSelected", expectAction)
                .expectIntRecord("Android.SelectFileDialogUploadMethods", expectMethod)
                .build();
    }

    @Test
    public void testUploadMethodLogging() {
        SelectFileDialog selectFileDialog = new SelectFileDialog(null);

        List<Object[]> testCases =
                Arrays.asList(
                        new Object[][] {
                            // Test cases for MIME-type lookup:
                            {
                                "foo.jpg",
                                "image/jpg",
                                /* useMediaPicker= */ true,
                                SelectFileDialog.FileSelectedAction.MEDIA_PICKER_IMAGE_BY_MIME_TYPE,
                                SelectFileDialog.FileSelectedUploadMethod.MEDIA_PICKER_IMAGE
                            },
                            {
                                "foo.jpg",
                                "image/jpg",
                                /* useMediaPicker= */ false,
                                SelectFileDialog.FileSelectedAction
                                        .EXTERNAL_PICKER_IMAGE_BY_MIME_TYPE,
                                SelectFileDialog.FileSelectedUploadMethod.EXTERNAL_PICKER_IMAGE
                            },
                            {
                                "foo.mp4",
                                "video/mp4",
                                /* useMediaPicker= */ true,
                                SelectFileDialog.FileSelectedAction.MEDIA_PICKER_VIDEO_BY_MIME_TYPE,
                                SelectFileDialog.FileSelectedUploadMethod.MEDIA_PICKER_VIDEO
                            },
                            {
                                "foo.mp4",
                                "video/mp4",
                                /* useMediaPicker= */ false,
                                SelectFileDialog.FileSelectedAction
                                        .EXTERNAL_PICKER_VIDEO_BY_MIME_TYPE,
                                SelectFileDialog.FileSelectedUploadMethod.EXTERNAL_PICKER_VIDEO
                            },
                            {
                                "foo.txt",
                                "text/plain",
                                /* useMediaPicker= */ true,
                                SelectFileDialog.FileSelectedAction.MEDIA_PICKER_OTHER_BY_MIME_TYPE,
                                SelectFileDialog.FileSelectedUploadMethod.MEDIA_PICKER_OTHER
                            },
                            {
                                "foo.txt",
                                "text/plain",
                                /* useMediaPicker= */ false,
                                SelectFileDialog.FileSelectedAction
                                        .EXTERNAL_PICKER_OTHER_BY_MIME_TYPE,
                                SelectFileDialog.FileSelectedUploadMethod.EXTERNAL_PICKER_OTHER
                            },
                            // Test cases for lookup by extension:
                            {
                                "foo.jpg",
                                null,
                                /* useMediaPicker= */ true,
                                SelectFileDialog.FileSelectedAction.MEDIA_PICKER_IMAGE_BY_EXTENSION,
                                SelectFileDialog.FileSelectedUploadMethod.MEDIA_PICKER_IMAGE
                            },
                            {
                                "foo.jpg",
                                null,
                                /* useMediaPicker= */ false,
                                SelectFileDialog.FileSelectedAction
                                        .EXTERNAL_PICKER_IMAGE_BY_EXTENSION,
                                SelectFileDialog.FileSelectedUploadMethod.EXTERNAL_PICKER_IMAGE
                            },
                            {
                                "foo.mp4",
                                null,
                                /* useMediaPicker= */ true,
                                SelectFileDialog.FileSelectedAction.MEDIA_PICKER_VIDEO_BY_EXTENSION,
                                SelectFileDialog.FileSelectedUploadMethod.MEDIA_PICKER_VIDEO
                            },
                            {
                                "foo.mp4",
                                null,
                                /* useMediaPicker= */ false,
                                SelectFileDialog.FileSelectedAction
                                        .EXTERNAL_PICKER_VIDEO_BY_EXTENSION,
                                SelectFileDialog.FileSelectedUploadMethod.EXTERNAL_PICKER_VIDEO
                            },
                            {
                                "foo.txt",
                                null,
                                /* useMediaPicker= */ true,
                                SelectFileDialog.FileSelectedAction.MEDIA_PICKER_OTHER_BY_EXTENSION,
                                SelectFileDialog.FileSelectedUploadMethod.MEDIA_PICKER_OTHER
                            },
                            {
                                "foo.txt",
                                null,
                                /* useMediaPicker= */ false,
                                SelectFileDialog.FileSelectedAction
                                        .EXTERNAL_PICKER_OTHER_BY_EXTENSION,
                                SelectFileDialog.FileSelectedUploadMethod.EXTERNAL_PICKER_OTHER
                            },

                            // Pathological (no filename -- results in URI parsing failing):
                            {
                                "",
                                null,
                                /* useMediaPicker= */ true,
                                SelectFileDialog.FileSelectedAction.MEDIA_PICKER_UNKNOWN_TYPE,
                                SelectFileDialog.FileSelectedUploadMethod.MEDIA_PICKER_UNKNOWN_TYPE
                            },
                            {
                                "",
                                null,
                                /* useMediaPicker= */ false,
                                SelectFileDialog.FileSelectedAction.EXTERNAL_PICKER_UNKNOWN_TYPE,
                                SelectFileDialog.FileSelectedUploadMethod
                                        .EXTERNAL_PICKER_UNKNOWN_TYPE
                            },
                            // Pathological (no MIME type and no extension):
                            {
                                "foo",
                                null,
                                /* useMediaPicker= */ true,
                                SelectFileDialog.FileSelectedAction.MEDIA_PICKER_UNKNOWN_TYPE,
                                SelectFileDialog.FileSelectedUploadMethod.MEDIA_PICKER_UNKNOWN_TYPE
                            },
                            {
                                "foo",
                                null,
                                /* useMediaPicker= */ false,
                                SelectFileDialog.FileSelectedAction.EXTERNAL_PICKER_UNKNOWN_TYPE,
                                SelectFileDialog.FileSelectedUploadMethod
                                        .EXTERNAL_PICKER_UNKNOWN_TYPE
                            },
                            // Pathological (ContentResolver throwing exception):
                            {
                                "foo",
                                "THROW",
                                /* useMediaPicker= */ true,
                                SelectFileDialog.FileSelectedAction.MEDIA_PICKER_UNKNOWN_TYPE,
                                SelectFileDialog.FileSelectedUploadMethod.MEDIA_PICKER_UNKNOWN_TYPE
                            },
                            {
                                "foo",
                                "THROW",
                                /* useMediaPicker= */ false,
                                SelectFileDialog.FileSelectedAction.EXTERNAL_PICKER_UNKNOWN_TYPE,
                                SelectFileDialog.FileSelectedUploadMethod
                                        .EXTERNAL_PICKER_UNKNOWN_TYPE
                            },
                        });

        for (Object[] testCase : testCases) {
            String filename = (String) testCase[0];
            String mimeType = (String) testCase[1];
            boolean useMediaPicker = (boolean) testCase[2];
            int action = (int) testCase[3];
            int method = (int) testCase[4];

            var histogramWatcher = getHistogramWatcher(action, method);
            String[] filesSelected = new String[] {filename};
            AsyncTask<Boolean> task =
                    selectFileDialog.getUploadMetricTaskForTesting(
                            mimeType != null
                                    ? getMockContentResolver(mimeType)
                                    : ContextUtils.getApplicationContext().getContentResolver(),
                            filesSelected,
                            useMediaPicker);
            task.executeOnExecutor(RobolectricUtil.getPausedExecutor());
            runAllAsyncTasks();
            histogramWatcher.assertExpected(
                    "File: "
                            + filename
                            + " MimeType: "
                            + mimeType
                            + " Action: "
                            + action
                            + " Method: "
                            + method);
        }
    }

    /**
     * TODO(b/281539662): Add a test for checking the caller permission when Robolectrics supports
     * Android V.
     */
    @Test
    @EnableFeatures({UiAndroidFeatures.CHECK_INTENT_CALLER_PERMISSION})
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testIntentPermissionNotCheckedOnLowerSDKVersions() throws Exception {
        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);
        selectFileDialog.onIntentCompleted(
                Activity.RESULT_OK,
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse("content://com.android.this_is_not_a_real_extension/xyz")));
    }

    @Test
    public void testExternalPickerAllMimeTypeDesktopDoesNotShowCameraIntents() throws Exception {
        DeviceInfo.setIsDesktopForTesting(true);
        assertTrue(DeviceInfo.isDesktop());

        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);

        // Mock camera intent resolution.
        IntentArgumentMatcher imageCaptureIntentArgumentMatcher =
                new IntentArgumentMatcher(MediaStore.ACTION_IMAGE_CAPTURE);
        when(windowAndroid.canResolveActivity(
                        ArgumentMatchers.argThat(imageCaptureIntentArgumentMatcher)))
                .thenReturn(true);
        when(windowAndroid.hasPermission(Manifest.permission.CAMERA)).thenReturn(true);

        // Setup WindowAndroid#showIntent to succeed (and validate the call).
        IntentArgumentMatcher chooserIntentArgumentMatcher =
                new IntentArgumentMatcher(Intent.ACTION_CHOOSER);
        Mockito.doAnswer(
                        (invocation) -> {
                            Intent chooserIntent = (Intent) invocation.getArguments()[0];
                            // On desktop, EXTRA_INITIAL_INTENTS should be null or empty
                            // because we stop injecting Camera intents.
                            assertFalse(chooserIntent.hasExtra(Intent.EXTRA_INITIAL_INTENTS));
                            return true;
                        })
                .when(windowAndroid)
                .showIntent(
                        ArgumentMatchers.argThat(chooserIntentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        selectFileDialog.selectFile(
                Intent.ACTION_GET_CONTENT,
                new String[] {"*/*"},
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);

        // We need to run async tasks because GetCameraIntentTask is executed.
        runAllAsyncTasks();
    }

    @Test
    public void testExternalPickerAllMimeTypeNonDesktopShowsCameraIntents() throws Exception {
        DeviceInfo.setIsDesktopForTesting(false);
        assertFalse(DeviceInfo.isDesktop());

        // When GetCameraIntentTask is ran it attempts to convert the temporary file on disk to a
        // content URI. To avoid idiosyncrasies with the Android content stack, just mock a return
        // URI here as the actual value doesn't matter entirely.
        FileProviderUtils.setFileProviderUtil(
                new FileProviderUtils.FileProviderUtil() {
                    @Override
                    public Uri getContentUriFromFile(File file) {
                        Uri.Builder builder = new Uri.Builder();
                        String fileString = file.toString();
                        if (fileString.startsWith("/")) {
                            fileString = fileString.substring(1);
                        }
                        builder.scheme("content").authority("org.chromium.test");
                        for (String path : fileString.split("/")) {
                            builder.appendPath(path);
                        }
                        return builder.build();
                    }
                });

        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);

        // Mock camera intent resolution.
        IntentArgumentMatcher imageCaptureIntentArgumentMatcher =
                new IntentArgumentMatcher(MediaStore.ACTION_IMAGE_CAPTURE);
        when(windowAndroid.canResolveActivity(
                        ArgumentMatchers.argThat(imageCaptureIntentArgumentMatcher)))
                .thenReturn(true);
        when(windowAndroid.hasPermission(Manifest.permission.CAMERA)).thenReturn(true);

        // Setup WindowAndroid#showIntent to succeed (and validate the call).
        IntentArgumentMatcher chooserIntentArgumentMatcher =
                new IntentArgumentMatcher(Intent.ACTION_CHOOSER);
        Mockito.doAnswer(
                        (invocation) -> {
                            Intent chooserIntent = (Intent) invocation.getArguments()[0];
                            // On non-desktop, EXTRA_INITIAL_INTENTS should be present
                            // because camera is supported and permitted.
                            assertTrue(chooserIntent.hasExtra(Intent.EXTRA_INITIAL_INTENTS));
                            Intent[] extraIntents =
                                    (Intent[]) chooserIntent.getExtra(Intent.EXTRA_INITIAL_INTENTS);
                            assertEquals(1, extraIntents.length);
                            assertEquals(
                                    MediaStore.ACTION_IMAGE_CAPTURE, extraIntents[0].getAction());
                            return true;
                        })
                .when(windowAndroid)
                .showIntent(
                        ArgumentMatchers.argThat(chooserIntentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        selectFileDialog.selectFile(
                Intent.ACTION_GET_CONTENT,
                new String[] {"*/*"},
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);

        runAllAsyncTasks();
    }

    @Test
    public void testExternalPickerMediaMimeTypeDesktopDoesNotUsePhotoPicker() throws Exception {
        DeviceInfo.setIsDesktopForTesting(true);
        assertTrue(DeviceInfo.isDesktop());

        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);

        // Mock camera intent resolution.
        IntentArgumentMatcher imageCaptureIntentArgumentMatcher =
                new IntentArgumentMatcher(MediaStore.ACTION_IMAGE_CAPTURE);
        when(windowAndroid.canResolveActivity(
                        ArgumentMatchers.argThat(imageCaptureIntentArgumentMatcher)))
                .thenReturn(true);

        // Setup WindowAndroid#showIntent to succeed and validate the chooser intent.
        IntentArgumentMatcher chooserIntentArgumentMatcher =
                new IntentArgumentMatcher(Intent.ACTION_CHOOSER);
        Mockito.doAnswer(
                        (invocation) -> {
                            Intent chooserIntent = (Intent) invocation.getArguments()[0];
                            Intent getContentIntent =
                                    (Intent) chooserIntent.getExtra(Intent.EXTRA_INTENT);
                            assertEquals("*/*", getContentIntent.getType());
                            String[] mimeTypes =
                                    (String[]) getContentIntent.getExtra(Intent.EXTRA_MIME_TYPES);
                            assertArrayEquals(
                                    new String[] {"image/*", "type/nonexistent"}, mimeTypes);
                            // On desktop, EXTRA_INITIAL_INTENTS should be null or empty.
                            assertFalse(chooserIntent.hasExtra(Intent.EXTRA_INITIAL_INTENTS));
                            return true;
                        })
                .when(windowAndroid)
                .showIntent(
                        ArgumentMatchers.argThat(chooserIntentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        selectFileDialog.selectFile(
                Intent.ACTION_GET_CONTENT,
                new String[] {"image/*"},
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);

        // On desktop, redundant camera/audio permissions should not be requested.
        Mockito.verify(windowAndroid, Mockito.never()).requestPermissions(any(), any());

        runAllAsyncTasks();
    }

    @Test
    public void testSelectFileBlockedByDelegate() throws Exception {
        TestSelectFileDialog selectFileDialog = new TestSelectFileDialog(null);
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);

        PhotoPickerDelegate delegate = Mockito.mock(PhotoPickerDelegate.class);
        when(delegate.shouldBlockFilePicker(windowAndroid)).thenReturn(true);
        SelectFileDialog.setPhotoPickerDelegateForTesting(delegate);

        int callCount = mOnActionCallback.getCallCount();
        selectFileDialog.selectFile(
                Intent.ACTION_GET_CONTENT,
                new String[] {"image/*"},
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                windowAndroid);
        mOnActionCallback.waitForCallback(callCount, 1);
        assertEquals(0, selectFileDialog.mFileSelectionSuccess);
        assertEquals(1, selectFileDialog.mFileSelectionAborted);
        Mockito.verify(windowAndroid, Mockito.never())
                .showIntent(any(Intent.class), any(), anyInt());
    }

    @Test
    public void testIsSelectingFile() {
        WindowAndroid window1 = Mockito.mock(WindowAndroid.class);
        WindowAndroid window2 = Mockito.mock(WindowAndroid.class);

        assertFalse(SelectFileDialog.isSelectingFile(window1));
        assertFalse(SelectFileDialog.isSelectingFile(window2));

        // Setup WindowAndroid#showIntent to succeed, so that the dialog stays active while it is
        // waiting for the result of the chooser intent.
        IntentArgumentMatcher chooserIntentArgumentMatcher =
                new IntentArgumentMatcher(Intent.ACTION_CHOOSER);
        doReturn(true)
                .when(window1)
                .showIntent(
                        ArgumentMatchers.argThat(chooserIntentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        // Note: This intentionally uses SelectFileDialog (as opposed to TestSelectFileDialog),
        // because the test subclass stubs out the onFile*Selected functions, which is where the
        // dialog unregisters itself as active.
        SelectFileDialog dialog = new SelectFileDialog(null);
        dialog.selectFile(
                Intent.ACTION_GET_CONTENT,
                new String[] {"text/plain"},
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                window1);

        assertTrue(SelectFileDialog.isSelectingFile(window1));
        assertFalse(SelectFileDialog.isSelectingFile(window2));

        // Simulate the user dismissing the picker without selecting a file.
        dialog.onIntentCompleted(Activity.RESULT_CANCELED, null);
        assertFalse(SelectFileDialog.isSelectingFile(window1));
        assertFalse(SelectFileDialog.isSelectingFile(window2));

        runAllAsyncTasks();
    }

    @Test
    public void testSelectFileBlockedWhenAlreadySelectingFile() {
        WindowAndroid window = Mockito.mock(WindowAndroid.class);
        IntentArgumentMatcher chooserIntentArgumentMatcher =
                new IntentArgumentMatcher(Intent.ACTION_CHOOSER);
        doReturn(true)
                .when(window)
                .showIntent(
                        ArgumentMatchers.argThat(chooserIntentArgumentMatcher),
                        (WindowAndroid.IntentCallback) any(),
                        anyInt());

        SelectFileDialog dialog1 = new SelectFileDialog(null);
        dialog1.selectFile(
                Intent.ACTION_GET_CONTENT,
                new String[] {"text/plain"},
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                window);

        assertTrue(SelectFileDialog.isSelectingFile(window));

        // Second attempt to open a file dialog on the same window while dialog1 is active.
        TestSelectFileDialog dialog2 = new TestSelectFileDialog(null);
        dialog2.selectFile(
                Intent.ACTION_GET_CONTENT,
                new String[] {"text/plain"},
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                window);

        // Second dialog is immediately aborted without showing a second intent.
        assertEquals(0, dialog2.mFileSelectionSuccess);
        assertEquals(1, dialog2.mFileSelectionAborted);
        Mockito.verify(window, Mockito.times(1))
                .showIntent(any(Intent.class), (WindowAndroid.IntentCallback) any(), anyInt());

        // The rejected request must not release the window: dialog1 is still showing, so further
        // requests (including the Ctrl+O shortcut, which consults isSelectingFile()) stay blocked.
        assertTrue(SelectFileDialog.isSelectingFile(window));

        dialog1.onIntentCompleted(Activity.RESULT_CANCELED, null);
        assertFalse(SelectFileDialog.isSelectingFile(window));

        // Once the window is free again, a new request proceeds normally.
        TestSelectFileDialog dialog3 = new TestSelectFileDialog(null);
        dialog3.selectFile(
                Intent.ACTION_GET_CONTENT,
                new String[] {"text/plain"},
                /* capture= */ false,
                /* multiple= */ false,
                /* defaultDirectory= */ null,
                /* suggestedName= */ null,
                window);
        assertEquals(0, dialog3.mFileSelectionAborted);
        assertTrue(SelectFileDialog.isSelectingFile(window));
        Mockito.verify(window, Mockito.times(2))
                .showIntent(any(Intent.class), (WindowAndroid.IntentCallback) any(), anyInt());

        runAllAsyncTasks();
    }
}
