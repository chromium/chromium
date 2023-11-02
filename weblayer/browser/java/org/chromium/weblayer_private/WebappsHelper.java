// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.webapps.WebappsUtils;

class WebappsHelper {
    private WebappsHelper() {}

    @CalledByNative
    public static void addShortcutToHomescreen(
            String id, String url, String userTitle, Bitmap icon, boolean isIconAdaptive) {
        Intent shortcutIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        WebappsUtils.addShortcutToHomescreen(id, userTitle, icon, isIconAdaptive, shortcutIntent);
    }
}
