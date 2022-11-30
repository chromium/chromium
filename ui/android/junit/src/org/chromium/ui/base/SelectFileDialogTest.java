// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doReturn;

import android.net.Uri;
import android.webkit.MimeTypeMap;

import androidx.core.content.ContextCompat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowMimeTypeMap;

import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.FileUtilsJni;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.File;
import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;

/**
 * Tests logic in the SelectFileDialog class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectFileDialogTest {
    @Mock
    FileUtils.Natives mFileUtilsMocks;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
    }

    /**
     * Returns the determined scope for the accepted |fileTypes|.
     */
    private int scopeForFileTypes(String... fileTypes) {
        SelectFileDialog instance = SelectFileDialog.create((long) 0 /* nativeSelectFileDialog */);
        instance.setFileTypesForTests(new ArrayList<String>(Arrays.asList(fileTypes)));

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
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes("image/*", "test/plain"));

        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_VIDEOS, scopeForFileTypes("video/*"));
        assertEquals(
                SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_VIDEOS, scopeForFileTypes("video/ogg"));
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes("video/*", "test/plain"));
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES,
                scopeForFileTypes("image/x-png", "image/gif", "image/jpeg"));
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes("image/x-png", "image/gif", "image/jpeg", "text/plain"));

        // Test image extensions only.
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES,
                scopeForFileTypes(".jpg", ".jpeg", ".png", ".gif", ".apng", ".tiff", ".tif", ".bmp",
                        ".pdf", ".xcf", ".webp"));
        // Test image extensions mixed with image MIME types.
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES,
                scopeForFileTypes(".JPG", ".jpeg", "image/gif", "image/jpeg"));
        // Image extensions mixed with image MIME types and other.
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes(".jpg", "image/gif", "text/plain"));
        // Video extensions only.
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_VIDEOS,
                scopeForFileTypes(".asf", ".avhcd", ".avi", ".flv", ".mov", ".mp4", ".mpeg", ".mpg",
                        ".swf", ".wmv", ".webm", ".mkv", ".divx"));
        // Video extensions and video MIME types.
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_VIDEOS,
                scopeForFileTypes(".avi", ".mp4", "video/ogg"));
        // Video extensions and video MIME types and other.
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes(".avi", ".mp4", "video/ogg", "text/plain"));

        // Non-image, non-video extension only.
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC, scopeForFileTypes(".doc"));

        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES_AND_VIDEOS,
                scopeForFileTypes("video/*", "image/*"));
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_IMAGES_AND_VIDEOS,
                scopeForFileTypes("image/jpeg", "video/ogg"));
        assertEquals(SelectFileDialog.SELECT_FILE_DIALOG_SCOPE_GENERIC,
                scopeForFileTypes("video/*", "image/*", "text/plain"));
    }

    @Test
    public void testPhotoPickerLaunchAndMimeTypes() {
        ShadowMimeTypeMap shadowMimeTypeMap = Shadows.shadowOf(MimeTypeMap.getSingleton());
        shadowMimeTypeMap.addExtensionMimeTypMapping("jpg", "image/jpeg");
        shadowMimeTypeMap.addExtensionMimeTypMapping("gif", "image/gif");
        shadowMimeTypeMap.addExtensionMimeTypMapping("txt", "text/plain");
        shadowMimeTypeMap.addExtensionMimeTypMapping("mpg", "video/mpeg");

        assertEquals("", SelectFileDialog.ensureMimeType(""));
        assertEquals("image/jpeg", SelectFileDialog.ensureMimeType(".jpg"));
        assertEquals("image/jpeg", SelectFileDialog.ensureMimeType("image/jpeg"));
        // Unknown extension, expect default response:
        assertEquals("application/octet-stream", SelectFileDialog.ensureMimeType(".flv"));

        assertEquals(null, SelectFileDialog.convertToSupportedPhotoPickerTypes(new ArrayList<>()));
        assertEquals(null, SelectFileDialog.convertToSupportedPhotoPickerTypes(Arrays.asList("")));
        assertEquals(null,
                SelectFileDialog.convertToSupportedPhotoPickerTypes(Arrays.asList("foo/bar")));
        assertEquals(Arrays.asList("image/jpeg"),
                SelectFileDialog.convertToSupportedPhotoPickerTypes(Arrays.asList(".jpg")));
        assertEquals(Arrays.asList("image/jpeg"),
                SelectFileDialog.convertToSupportedPhotoPickerTypes(Arrays.asList("image/jpeg")));
        assertEquals(Arrays.asList("image/jpeg"),
                SelectFileDialog.convertToSupportedPhotoPickerTypes(
                        Arrays.asList(".jpg", "image/jpeg")));
        assertEquals(Arrays.asList("image/gif", "image/jpeg"),
                SelectFileDialog.convertToSupportedPhotoPickerTypes(
                        Arrays.asList(".gif", "image/jpeg")));

        // Video and mixed video/images support. This feature is supported, but off by default, so
        // expect failure until it is turned on by default.
        assertEquals(
                null, SelectFileDialog.convertToSupportedPhotoPickerTypes(Arrays.asList(".mpg")));
        assertEquals(null,
                SelectFileDialog.convertToSupportedPhotoPickerTypes(Arrays.asList("video/mpeg")));
        assertEquals(null,
                SelectFileDialog.convertToSupportedPhotoPickerTypes(
                        Arrays.asList(".jpg", "image/jpeg", ".mpg")));

        // Returns null because generic picker is required (due to addition of .txt file).
        assertEquals(null,
                SelectFileDialog.convertToSupportedPhotoPickerTypes(
                        Arrays.asList(".txt", ".jpg", "image/jpeg")));
    }

    @Test
    public void testMultipleFileSelectorWithFileUris() {
        SelectFileDialog selectFileDialog = new SelectFileDialog(0);
        Uri[] filePathArray = new Uri[] {
                Uri.parse("file:///storage/emulated/0/DCIM/Camera/IMG_0.jpg"),
                Uri.parse("file:///storage/emulated/0/DCIM/Camera/IMG_1.jpg")};
        SelectFileDialog.GetDisplayNameTask task = selectFileDialog.new GetDisplayNameTask(
                ContextUtils.getApplicationContext(), true, filePathArray);
        task.doInBackground();
        assertEquals(task.mFilePaths[0].toString(),
                "///storage/emulated/0/DCIM/Camera/IMG_0.jpg");
        assertEquals(task.mFilePaths[1].toString(),
                "///storage/emulated/0/DCIM/Camera/IMG_1.jpg");
    }

    private void testFilePath(
            String path, SelectFileDialog selectFileDialog, boolean expectedPass) {
        testFilePath(path, selectFileDialog, expectedPass, expectedPass);
    }

    private void testFilePath(String path, SelectFileDialog selectFileDialog,
            boolean expectedFileSelectionResult, boolean expectedGetDisplayNameResult) {
        Uri[] uris = new Uri[1];
        uris[0] = Uri.fromFile(new File(path));

        SelectFileDialog.FilePathSelectedTask task = selectFileDialog.new FilePathSelectedTask(
                ContextUtils.getApplicationContext(), path, null);
        SelectFileDialog.GetDisplayNameTask task2 =
                selectFileDialog.new GetDisplayNameTask(ContextUtils.getApplicationContext(),
                        /* isMultiple = */ false, uris);
        assertEquals(expectedFileSelectionResult, task.doInBackground());
        assertEquals(expectedGetDisplayNameResult, null != task2.doInBackground());
    }

    @Test
    public void testFilePathTasks() throws IOException {
        FileUtilsJni.TEST_HOOKS.setInstanceForTesting(mFileUtilsMocks);
        doReturn("/tmp/xyz.jpn").when(mFileUtilsMocks).getAbsoluteFilePath(any());

        SelectFileDialog selectFileDialog = new SelectFileDialog(0);

        // Obtain the data directory for RoboElectric. It should look something like:
        //   /tmp/robolectric-Method_[testName][number]/org.chromium.test.ui-dataDir
        // ... where [testName] is the name of this test function and [number] is a unique id.
        String dataDir =
                ContextCompat.getDataDir(ContextUtils.getApplicationContext()).getCanonicalPath();

        // Passing in the data directory itself should fail.
        testFilePath(dataDir, selectFileDialog, /* expectedPass= */ false);
        // Passing in a subdirectory of the data directory should also fail.
        testFilePath(dataDir + "/tmp/xyz.jpg", selectFileDialog, /* expectedPass= */ false);
        // The parent directory of the data directory should, however, succeed.
        testFilePath(dataDir + "/../xyz.jpg", selectFileDialog, /* expectedPass= */ true);
        // Another way of specifying the data directory (should fail).
        testFilePath(dataDir + "/tmp/../xyz.jpg", selectFileDialog, /* expectedPass= */ false);
        // The directory outside the data directory should succeed.
        testFilePath("/data/local/tmp.jpg", selectFileDialog, /* expectedPass= */ true);

        Path path = new File(dataDir).toPath();
        String parent = path.getParent().toString();
        String lastComponent = path.getName(path.getNameCount() - 1).toString();

        // Make sure that base/./dataDir is treated the same as base/dataDir (and fail the request).
        testFilePath(parent + "/./" + lastComponent + "/xyz.jpg", selectFileDialog,
                /* expectedPass= */ false);
        // Make sure that dataDir/../dataDir is treated the same as dataDir (and fail the request).
        testFilePath(dataDir + "/../" + lastComponent + "/xyz.jpg", selectFileDialog,
                /* expectedPass= */ false);

        // Tests invalid file path should fail file selection.
        doReturn(new String()).when(mFileUtilsMocks).getAbsoluteFilePath(any());
        testFilePath("\\/tmp/xyz.jpg", selectFileDialog,
                /* expectedFileSelectionResult= */ false, /* expectedGetDisplayNameResult= */ true);
    }

    @Test
    public void testShowTypes() {
        SelectFileDialog selectFileDialog = new SelectFileDialog(0);

        selectFileDialog.setFileTypesForTests(Arrays.asList("image/jpeg"));
        assertTrue(selectFileDialog.acceptsSingleType());
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("image/jpeg", "image/png"));
        assertFalse(selectFileDialog.acceptsSingleType());
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("image/*", "image/jpeg"));
        // Note: image/jpeg is part of image/* so this counts as a single type.
        assertTrue(selectFileDialog.acceptsSingleType());
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("image/*", "video/mp4"));
        assertFalse(selectFileDialog.acceptsSingleType());
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("image/jpeg", "video/mp4"));
        assertFalse(selectFileDialog.acceptsSingleType());
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("video/mp4"));
        assertTrue(selectFileDialog.acceptsSingleType());
        assertFalse(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("video/mp4", "video/*"));
        // Note: video/mp4 is part of video/* so this counts as a single type.
        assertTrue(selectFileDialog.acceptsSingleType());
        assertFalse(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("audio/wave", "audio/mpeg", "audio/*"));
        // Note: both audio/wave and audio/mpeg are part of audio/* so this counts as a single type.
        assertTrue(selectFileDialog.acceptsSingleType());
        assertFalse(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertTrue(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("audio/wave", "audio/mpeg"));
        assertFalse(selectFileDialog.acceptsSingleType());
        assertFalse(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertTrue(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("*/*"));
        assertFalse(selectFileDialog.acceptsSingleType());
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertTrue(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Collections.emptyList());
        assertFalse(selectFileDialog.acceptsSingleType());
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertTrue(selectFileDialog.shouldShowVideoTypes());
        assertTrue(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("image//png", "image/", "image"));
        assertFalse(selectFileDialog.acceptsSingleType());
        assertTrue(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("/image", "/"));
        assertFalse(selectFileDialog.acceptsSingleType());
        assertFalse(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());

        selectFileDialog.setFileTypesForTests(Arrays.asList("/", ""));
        assertFalse(selectFileDialog.acceptsSingleType());
        assertFalse(selectFileDialog.shouldShowImageTypes());
        assertFalse(selectFileDialog.shouldShowVideoTypes());
        assertFalse(selectFileDialog.shouldShowAudioTypes());
    }
}
