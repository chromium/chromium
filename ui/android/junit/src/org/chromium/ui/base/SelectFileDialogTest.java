// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;

import android.net.Uri;
import android.webkit.MimeTypeMap;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowMimeTypeMap;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Tests logic in the SelectFileDialog class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectFileDialogTest {
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
}
