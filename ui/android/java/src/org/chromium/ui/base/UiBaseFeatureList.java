// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.build.annotations.MainDex;

/**
 * Java accessor for ui/base/ui_base_feature_list.cc state.
 */
@JNINamespace("features")
@MainDex
public final class UiBaseFeatureList {
    // Do not instantiate this class.
    private UiBaseFeatureList() {}

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in ui/base/ui_base_feature_list_android.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     *
     */
    public static boolean isEnabled(String featureName) {
        return UiBaseFeatureListJni.get().isEnabled(featureName);
    }

    @NativeMethods
    interface Natives {
        boolean isEnabled(String featureName);
    }
}
