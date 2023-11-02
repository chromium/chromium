// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import java.util.List;

/** A delegate interface for the photo picker. */
public interface PhotoPickerDelegate {
    /**
     * Called to display the photo picker.
     * @param windowAndroid The window of the Activity.
     * @param listener The listener that will be notified of the action the user took in the
     *                 picker.
     * @param allowMultiple Whether the dialog should allow multiple images to be selected.
     * @param mimeTypes A list of mime types to show in the dialog.
     * @return the PhotoPicker object.
     */
    PhotoPicker showPhotoPicker(WindowAndroid windowAndroid, PhotoPickerListener listener,
            boolean allowMultiple, List<String> mimeTypes);

    /**
     * Returns whether video decoding support is supported in the photo picker.
     */
    boolean supportsVideos();

    /**
     * Returns whether the Android media picker is preferred over the Chrome media picker.
     */
    boolean preferAndroidMediaPicker();
}
