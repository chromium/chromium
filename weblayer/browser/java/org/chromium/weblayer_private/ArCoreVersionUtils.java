// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.pm.PackageManager;
import android.os.Bundle;

import org.chromium.base.PackageUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

@JNINamespace("weblayer")
class ArCoreVersionUtils {
    // Corresponds to V1.32. Must be updated if the arcore version in
    // //third_party/arcore-android-sdk-client is rolled.
    private static final int MIN_APK_VERSION = 221020000;

    private static final String AR_CORE_PACKAGE = "com.google.ar.core";
    private static final String METADATA_KEY_MIN_APK_VERSION = "com.google.ar.core.min_apk_version";

    @CalledByNative
    public static boolean isEnabled() {
        // If the appropriate metadata entries are not present in the app manifest, the feature
        // should be disabled.
        Bundle metaData = PackageUtils.getApplicationPackageInfo(PackageManager.GET_META_DATA)
                                  .applicationInfo.metaData;
        return metaData.containsKey(AR_CORE_PACKAGE)
                && metaData.containsKey(METADATA_KEY_MIN_APK_VERSION) && isInstalledAndCompatible();
    }

    @CalledByNative
    public static boolean isInstalledAndCompatible() {
        return PackageUtils.getPackageVersion(AR_CORE_PACKAGE) >= MIN_APK_VERSION;
    }
}
