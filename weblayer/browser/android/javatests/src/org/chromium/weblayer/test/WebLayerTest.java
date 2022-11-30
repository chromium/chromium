// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.File;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the WebLayer class.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class WebLayerTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private class GenerateUriCallback extends CallbackHelper implements Callback<Uri> {
        private Uri mImageUri;

        public Uri getImageUri() {
            return mImageUri;
        }

        @Override
        public void onResult(Uri uri) {
            mImageUri = uri;
            notifyCalled();
        }
    }

    @Test
    @SmallTest
    public void getUserAgentString() {
        final String userAgent = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return mActivityTestRule.getWebLayer().getUserAgentString(); });
        Assert.assertNotNull(userAgent);
        Assert.assertFalse(userAgent.isEmpty());
    }

    @MinWebLayerVersion(94)
    @Test
    @SmallTest
    public void deletesTemporaryImagesOnInit() throws TimeoutException {
        // Create a temporary image to simulate an old file left from a previous share.
        GenerateUriCallback imageCallback = new GenerateUriCallback();
        ShareImageFileUtils.generateTemporaryUriFromData(new byte[] {1, 2}, ".png", imageCallback);
        imageCallback.waitForCallback(0, 1, 30L, TimeUnit.SECONDS);
        File imageFile = new File(imageCallback.getImageUri().getPath());

        // Verify that the file now exists.
        Assert.assertTrue(imageFile.exists());

        // Create a WebLayer instance which should delete any previously generated images.
        mActivityTestRule.getWebLayer();

        // Verify that the file has been deleted. Note that this happens in the background.
        CriteriaHelper.pollInstrumentationThread(
                () -> Criteria.checkThat(imageFile.exists(), Matchers.is(false)));
    }
}
