// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.doReturn;
import static org.robolectric.Shadows.shadowOf;

import android.Manifest.permission;
import android.os.Build.VERSION_CODES;
import android.webkit.MimeTypeMap;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;

/** Tests for {@link MimeTypeUtils}, verifying behavior across OS versions. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // Mocking GURL
public class MimeTypeUtilsTest {
    @Mock private GURL mMockedUrl;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testGetMimeTypeFromUrlText() {
        updateMockGurlSpec("file://file.html");
        shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypeMapping("html", "text/html");
        assertEquals(
                "Expecting text mime type",
                MimeTypeUtils.Type.TEXT,
                MimeTypeUtils.getMimeTypeForUrl(mMockedUrl));
    }

    @Test
    public void testGetMimeTypeFromUrlImage() {
        updateMockGurlSpec("file://file.jpg");
        shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypeMapping("jpg", "image/jpeg");
        assertEquals(
                "Expecting image mime type",
                MimeTypeUtils.Type.IMAGE,
                MimeTypeUtils.getMimeTypeForUrl(mMockedUrl));
    }

    @Test
    public void testGetMimeTypeFromUrlAudio() {
        updateMockGurlSpec("file://file.mp3");
        shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypeMapping("mp3", "audio/mpeg");
        assertEquals(
                "Expecting audio mime type",
                MimeTypeUtils.Type.AUDIO,
                MimeTypeUtils.getMimeTypeForUrl(mMockedUrl));
    }

    @Test
    public void testGetMimeTypeFromUrlVideo() {
        updateMockGurlSpec("file://file.mp4");
        shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypeMapping("mp4", "video/mp4");
        assertEquals(
                "Expecting video mime type",
                MimeTypeUtils.Type.VIDEO,
                MimeTypeUtils.getMimeTypeForUrl(mMockedUrl));
    }

    @Test
    public void testGetMimeTypeFromUrlPDF() {
        updateMockGurlSpec("file://file.pdf");
        shadowOf(MimeTypeMap.getSingleton()).addExtensionMimeTypeMapping("pdf", "application/pdf");
        assertEquals(
                "Expecting PDF mime type",
                MimeTypeUtils.Type.PDF,
                MimeTypeUtils.getMimeTypeForUrl(mMockedUrl));
    }

    @Test
    public void testGetMimeTypeFromUrlUnknown() {
        updateMockGurlSpec("file://file.foo");
        assertEquals(
                "Expecting unknown mime type",
                MimeTypeUtils.Type.UNKNOWN,
                MimeTypeUtils.getMimeTypeForUrl(mMockedUrl));

        updateMockGurlSpec("file://file");
        assertEquals(
                "Expecting unknown mime type for file with no extension",
                MimeTypeUtils.Type.UNKNOWN,
                MimeTypeUtils.getMimeTypeForUrl(mMockedUrl));
    }

    @Test
    @Config(sdk = VERSION_CODES.Q)
    public void testPermissionForMimeTypePreAndroidT() {
        assertEquals(
                "Wrong permission for audio mime type",
                permission.READ_EXTERNAL_STORAGE,
                MimeTypeUtils.getPermissionNameForMimeType(MimeTypeUtils.Type.AUDIO));
        assertEquals(
                "Wrong permission for pdf mime type",
                permission.READ_EXTERNAL_STORAGE,
                MimeTypeUtils.getPermissionNameForMimeType(MimeTypeUtils.Type.PDF));
    }

    @Test
    @Config(shadows = {ShadowMimeTypeUtilsForT.class})
    public void testPermissionForMimeTypeAndroidT() {
        assertEquals(
                "Wrong permission for audio mime type",
                permission.READ_MEDIA_AUDIO,
                MimeTypeUtils.getPermissionNameForMimeType(MimeTypeUtils.Type.AUDIO));
        assertEquals(
                "Wrong permission for image mime type",
                permission.READ_MEDIA_IMAGES,
                MimeTypeUtils.getPermissionNameForMimeType(MimeTypeUtils.Type.IMAGE));
        assertEquals(
                "Wrong permission for video mime type",
                permission.READ_MEDIA_VIDEO,
                MimeTypeUtils.getPermissionNameForMimeType(MimeTypeUtils.Type.VIDEO));
        assertNull(
                "Wrong permission for pdf mime type",
                MimeTypeUtils.getPermissionNameForMimeType(MimeTypeUtils.Type.PDF));
    }

    private void updateMockGurlSpec(String spec) {
        doReturn(spec).when(mMockedUrl).getSpec();
    }

    @Implements(MimeTypeUtils.class)
    private static class ShadowMimeTypeUtilsForT {
        @Implementation
        public static boolean useExternalStoragePermission() {
            return false;
        }
    }
}
