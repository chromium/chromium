// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

/**
 * Callback for capturing screenshots.
 */
public interface CaptureScreenShotCallback {
    /**
     * @param bitmap The result bitmap. May be null on failure
     * @param errorCode An opaque error code value for debugging. 0 indicates success.
     */
    void onScreenShotCaptured(@Nullable Bitmap bitmap, int errorCode);
}
