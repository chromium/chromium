// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

/**
 * An interface for the custom image file picker.
 * See {@link SelectFileDialog}.
 */
public interface PhotoPicker {
    /**
     * Called after use of the PhotoPicker results in an external intent.
     * When the PhotoPicker is used to choose the camera, for example, {@link SelectFileDialog} will
     * launch a camera intent. When that intent is done, this will be called. This allows the
     * PhotoPicker to defer dismissing until the Camera intent has been shown and completed.
     */
    void onExternalIntentCompleted();
}
