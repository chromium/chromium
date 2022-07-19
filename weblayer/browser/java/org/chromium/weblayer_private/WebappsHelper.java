// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.graphics.Bitmap;

import org.chromium.base.annotations.CalledByNative;

class WebappsHelper {
    private WebappsHelper() {}

    @CalledByNative
    public static void addShortcutToHomescreen(
            String id, String url, String userTitle, Bitmap icon, boolean isIconAdaptive) {
    }
}
