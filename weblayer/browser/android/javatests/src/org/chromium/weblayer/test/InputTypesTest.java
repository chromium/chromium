// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.Manifest;
import android.app.Activity;
import android.content.ClipData;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Parcelable;
import android.provider.MediaStore;

import androidx.annotation.RequiresApi;
import androidx.core.app.ActivityCompat;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.ApplicationContextWrapper;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.io.File;
import java.util.Arrays;

/**
 * Tests that file inputs work as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class InputTypesTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private File mTempFile;
    private File mTestDir;
    private int mCameraPermission = PackageManager.PERMISSION_GRANTED;

    private class FileIntentInterceptor implements InstrumentationActivity.IntentInterceptor {
        public Intent mLastIntent;

        private Intent mResponseIntent;
        private int mResultCode = Activity.RESULT_CANCELED;
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public void interceptIntent(Intent intent, int requestCode, Bundle options) {
            new Handler().post(() -> {
                mActivityTestRule.getActivity().getActivityResultRegistry().dispatchResult(
                        requestCode, mResultCode, mResponseIntent);
                mLastIntent = intent;
                mCallbackHelper.notifyCalled();
            });
        }

        public void waitForIntent() {
            try {
                mCallbackHelper.waitForCallback(0);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public void setResponse(int resultCode, Intent response) {
            mResponseIntent = response;
            mResultCode = resultCode;
        }
    }

    private FileIntentInterceptor mIntentInterceptor = new FileIntentInterceptor();

    @RequiresApi(Build.VERSION_CODES.M)
    private class PermissionCompatDelegate implements ActivityCompat.PermissionCompatDelegate {
        public int mResult = PackageManager.PERMISSION_DENIED;
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public boolean requestPermissions(
                Activity activity, String[] permissions, int requestCode) {
            new Handler().post(() -> {
                int[] results = new int[permissions.length];
                Arrays.fill(results, mResult);
                mCameraPermission = mResult;
                activity.onRequestPermissionsResult(requestCode, permissions, results);
                mCallbackHelper.notifyCalled();
            });
            return true;
        }

        @Override
        public boolean onActivityResult(
                Activity activity, int requestCode, int resultCode, Intent data) {
            return false;
        }

        public void waitForPermissionsRequest() {
            try {
                mCallbackHelper.waitForCallback(0);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public void setResult(int result) {
            mResult = result;
        }
    }

    private PermissionCompatDelegate mPermissionCompatDelegate = new PermissionCompatDelegate();

    @Before
    public void setUp() throws Exception {
        ContextUtils.initApplicationContextForTests(
                new ApplicationContextWrapper(ContextUtils.getApplicationContext()) {
                    @Override
                    public int checkPermission(String permission, int pid, int uid) {
                        if (permission.equals(Manifest.permission.CAMERA)) {
                            return mCameraPermission;
                        }
                        return super.checkPermission(permission, pid, uid);
                    }
                });
        ActivityCompat.setPermissionCompatDelegate(mPermissionCompatDelegate);

        mTestDir = new File(UrlUtils.getIsolatedTestFilePath("weblayer"));
        if (!mTestDir.exists()) mTestDir.mkdir();
        mTempFile = File.createTempFile("file", null, mTestDir);
        Intent response = new Intent();
        response.setData(Uri.fromFile(mTempFile));
        mIntentInterceptor.setResponse(Activity.RESULT_OK, response);

        Bundle extras = new Bundle();
        // We need to override the context with which to create WebLayer.
        extras.putBoolean(InstrumentationActivity.EXTRA_CREATE_WEBLAYER, false);
        InstrumentationActivity activity = mActivityTestRule.launchShell(extras);
        activity.setIntentInterceptor(mIntentInterceptor);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.loadWebLayerSync(ContextUtils.getApplicationContext()));
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL("input_types.html"));
    }

    @After
    public void tearDown() {
        ActivityCompat.setPermissionCompatDelegate(null);
        if (mTempFile != null) {
            mTempFile.delete();
        }
        if (mTestDir != null) {
            mTestDir.delete();
        }
    }

    @Test
    @SmallTest
    public void testFileInputBasic() {
        String id = "input_file";

        openFileInputWithId(id);

        Assert.assertFalse(getContentIntent().hasCategory(Intent.CATEGORY_OPENABLE));

        waitForNumFiles(id, 1);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void testFileInputCameraPermissionGranted() throws Exception {
        mCameraPermission = PackageManager.PERMISSION_DENIED;
        mPermissionCompatDelegate.setResult(PackageManager.PERMISSION_GRANTED);
        String id = "input_file";

        openFileInputWithId(id);
        mPermissionCompatDelegate.waitForPermissionsRequest();

        Parcelable[] intents = mIntentInterceptor.mLastIntent.getParcelableArrayExtra(
                Intent.EXTRA_INITIAL_INTENTS);
        Assert.assertFalse(intents.length == 0);
        Assert.assertEquals(MediaStore.ACTION_IMAGE_CAPTURE, ((Intent) intents[0]).getAction());

        waitForNumFiles(id, 1);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    public void testFileInputCameraPermissionDenied() throws Exception {
        mCameraPermission = PackageManager.PERMISSION_DENIED;
        mPermissionCompatDelegate.setResult(PackageManager.PERMISSION_DENIED);
        String id = "input_file";

        openFileInputWithId(id);
        mPermissionCompatDelegate.waitForPermissionsRequest();

        Parcelable[] intents = mIntentInterceptor.mLastIntent.getParcelableArrayExtra(
                Intent.EXTRA_INITIAL_INTENTS);
        for (Parcelable intent : intents) {
            Assert.assertNotEquals(MediaStore.ACTION_IMAGE_CAPTURE, ((Intent) intent).getAction());
        }

        waitForNumFiles(id, 1);
    }

    @Test
    @SmallTest
    public void testFileInputCancel() {
        String id = "input_file";

        // First add a file.
        openFileInputWithId(id);
        waitForNumFiles(id, 1);

        // Now cancel the intent.
        mIntentInterceptor.setResponse(Activity.RESULT_CANCELED, null);
        openFileInputWithId(id);
        waitForNumFiles(id, 0);
    }

    @Test
    @SmallTest
    public void testFileInputText() {
        String id = "input_text";

        openFileInputWithId(id);

        Assert.assertTrue(getContentIntent().hasCategory(Intent.CATEGORY_OPENABLE));

        waitForNumFiles(id, 1);
    }

    @Test
    @SmallTest
    public void testFileInputAny() {
        String id = "input_any";

        openFileInputWithId(id);

        Assert.assertFalse(getContentIntent().hasCategory(Intent.CATEGORY_OPENABLE));

        waitForNumFiles(id, 1);
    }

    @Test
    @SmallTest
    public void testFileInputMultiple() throws Exception {
        Intent response = new Intent();
        ClipData clipData = ClipData.newUri(mActivityTestRule.getActivity().getContentResolver(),
                "uris", Uri.fromFile(mTempFile));
        File otherTempFile = File.createTempFile("file2", null, mTestDir);
        clipData.addItem(new ClipData.Item(Uri.fromFile(otherTempFile)));
        response.setClipData(clipData);
        mIntentInterceptor.setResponse(Activity.RESULT_OK, response);
        String id = "input_file_multiple";

        openFileInputWithId(id);

        Intent contentIntent = getContentIntent();
        Assert.assertFalse(contentIntent.hasCategory(Intent.CATEGORY_OPENABLE));
        Assert.assertTrue(contentIntent.hasExtra(Intent.EXTRA_ALLOW_MULTIPLE));

        waitForNumFiles(id, 2);
        otherTempFile.delete();
    }

    @Test
    @SmallTest
    public void testFileInputImage() {
        String id = "input_image";

        openFileInputWithId(id);

        Assert.assertEquals(
                MediaStore.ACTION_IMAGE_CAPTURE, mIntentInterceptor.mLastIntent.getAction());

        waitForNumFiles(id, 1);
    }

    @Test
    @SmallTest
    public void testFileInputAudio() {
        String id = "input_audio";

        openFileInputWithId(id);

        Assert.assertEquals(MediaStore.Audio.Media.RECORD_SOUND_ACTION,
                mIntentInterceptor.mLastIntent.getAction());

        waitForNumFiles(id, 1);
    }

    @Test
    @SmallTest
    public void testColorInput() {
        // Just make sure we don't crash when opening the color picker.
        mActivityTestRule.executeScriptSync("var done = false; document.onclick = function() {"
                        + "document.getElementById('input_color').click(); done = true;}",
                true /* useSeparateIsolate */);
        EventUtils.simulateTouchCenterOfView(
                mActivityTestRule.getActivity().getWindow().getDecorView());
        CriteriaHelper.pollInstrumentationThread(
                () -> { return mActivityTestRule.executeScriptAndExtractBoolean("done"); });
    }

    private void openFileInputWithId(String id) {
        // We need to click the input after user input, otherwise it won't open due to security
        // policy.
        mActivityTestRule.executeScriptSync(
                "document.onclick = function() {document.getElementById('" + id + "').click()}",
                true /* useSeparateIsolate */);
        EventUtils.simulateTouchCenterOfView(
                mActivityTestRule.getActivity().getWindow().getDecorView());
        mIntentInterceptor.waitForIntent();
    }

    private void waitForNumFiles(String id, int num) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            int actual = mActivityTestRule.executeScriptAndExtractInt(
                    "document.getElementById('" + id + "').files.length");
            Criteria.checkThat(actual, Matchers.is(num));
        });
    }

    private Intent getContentIntent() {
        Assert.assertEquals(Intent.ACTION_CHOOSER, mIntentInterceptor.mLastIntent.getAction());
        Intent contentIntent =
                (Intent) mIntentInterceptor.mLastIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        Assert.assertNotNull(contentIntent);
        return contentIntent;
    }
}
