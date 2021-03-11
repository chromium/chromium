// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/** Provides feature enable/disable state for BackgroundFetch. */
@JNINamespace("weblayer")
public class BackgroundFetchHelper {
    private BackgroundFetchHelper() {}

    /**
     * Returns whether background fetch should be enabled.
     *
     * Embedding apps can control this with a manifest metadata tag. The default is false.
     */
    @CalledByNative
    public static boolean isEnabled() {
        Context context = ContextUtils.getApplicationContext();
        try {
            ApplicationInfo info = context.getPackageManager().getApplicationInfo(
                    context.getPackageName(), PackageManager.GET_META_DATA);
            return info.metaData.getBoolean("org.chromium.weblayer.ENABLE_BACKGROUND_FETCH", false);
        } catch (NameNotFoundException exception) {
            return false;
        }
    }
}
