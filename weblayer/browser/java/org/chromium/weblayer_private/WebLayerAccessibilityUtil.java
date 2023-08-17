// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.components.browser_ui.accessibility.FontSizePrefs;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.util.AccessibilityUtil;

/**
 * Exposes information about the current accessibility state.
 */
public class WebLayerAccessibilityUtil extends AccessibilityUtil {
    private static WebLayerAccessibilityUtil sInstance;

    public static WebLayerAccessibilityUtil get() {
        if (sInstance == null) {
            sInstance = new WebLayerAccessibilityUtil();
            AccessibilityState.addListener(sInstance);
        }
        return sInstance;
    }

    private WebLayerAccessibilityUtil() {}

    public void onBrowserResumed(ProfileImpl profile) {
        // When a browser is resumed the cached state may have be stale and needs to be
        // recalculated.
        FontSizePrefs.getInstance(profile).onSystemFontScaleChanged();
    }
}
