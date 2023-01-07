// Copyright 2021 The Chromium Authors
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

/** Exposes values of metadata tags to native code. */
@JNINamespace("weblayer")
public class ApplicationInfoHelper {
    private ApplicationInfoHelper() {}

    /**
     * Returns the boolean value for a metadata tag in the application's manifest.
     */
    @CalledByNative
    public static boolean getMetadataAsBoolean(String metadataTag, boolean defaultValue) {
        Context context = ContextUtils.getApplicationContext();
        try {
            ApplicationInfo info = context.getPackageManager().getApplicationInfo(
                    context.getPackageName(), PackageManager.GET_META_DATA);
            return info.metaData.getBoolean(metadataTag, defaultValue);
        } catch (NameNotFoundException exception) {
            return defaultValue;
        }
    }
}
