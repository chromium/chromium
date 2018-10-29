// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The callback used to indicate what action the user took in the picker.
 */
public interface PhotoPickerListener {
    /**
     * The action the user took in the picker.
     */
    @IntDef({PhotoPickerAction.CANCEL, PhotoPickerAction.PHOTOS_SELECTED,
            PhotoPickerAction.LAUNCH_CAMERA, PhotoPickerAction.LAUNCH_GALLERY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PhotoPickerAction {
        int CANCEL = 0;
        int PHOTOS_SELECTED = 1;
        int LAUNCH_CAMERA = 2;
        int LAUNCH_GALLERY = 3;
        int NUM_ENTRIES = 4;
    }

    /**
     * The types of requests supported.
     */
    static final int TAKE_PHOTO_REQUEST = 1;
    static final int SHOW_GALLERY = 2;

    /**
     * Called when the user has selected an action. For possible actions see above.
     *
     * @param photos The photos that were selected.
     */
    void onPhotoPickerUserAction(@PhotoPickerAction int action, String[] photos);
}
