// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.build.annotations.MainDex;

/**
 * Java accessor for ui/android/ui_android_feature_list.cc state
 */
@JNINamespace("ui")
@MainDex
public class UiAndroidFeatureList {
    // Do not instantiate this class
    private UiAndroidFeatureList() {}

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in ui/android/ui_android_feature_list.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        return UiAndroidFeatureListJni.get().isEnabled(featureName);
    }

    @NativeMethods
    public interface Natives {
        boolean isEnabled(String featureName);
    }
}
