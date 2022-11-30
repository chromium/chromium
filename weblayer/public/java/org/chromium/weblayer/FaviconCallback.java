// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

/**
 * Informed of changes to the favicon of the current navigation.
 */
abstract class FaviconCallback {
    /**
     * Called when the favicon of the current navigation has changed. This is called with null when
     * a navigation is started.
     *
     * @param favicon The favicon.
     */
    public void onFaviconChanged(@Nullable Bitmap favicon) {}
}
