// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

@JNINamespace("weblayer")
class ArCoreVersionUtils {
    // Corresponds to V1.22. Must be updated if the arcore version in
    // //third_party/arcore-android-sdk-client is rolled.
    private static final int MIN_APK_VERSION = 202940000;

    private static final String AR_CORE_PACKAGE = "com.google.ar.core";
    private static final String METADATA_KEY_MIN_APK_VERSION = "com.google.ar.core.min_apk_version";

    @CalledByNative
    public static boolean isEnabled() {
        Context context = ContextUtils.getApplicationContext();
        PackageManager pm = context.getPackageManager();
        try {
            // If the appropriate metadata entries are not present in the app manifest, the feature
            // should be disabled.
            Bundle metaData =
                    pm.getApplicationInfo(context.getPackageName(), PackageManager.GET_META_DATA)
                            .metaData;
            return metaData.containsKey(AR_CORE_PACKAGE)
                    && metaData.containsKey(METADATA_KEY_MIN_APK_VERSION)
                    && isInstalledAndCompatible();
        } catch (PackageManager.NameNotFoundException e) {
        }
        return false;
    }

    @CalledByNative
    public static boolean isInstalledAndCompatible() {
        return PackageUtils.getPackageVersion(ContextUtils.getApplicationContext(), AR_CORE_PACKAGE)
                >= MIN_APK_VERSION;
    }
}
