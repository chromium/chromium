// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.cached_flags.CachedFlag;

import java.util.List;

/** Helpers and state for features from {@link UiAndroidFeatures}. */
@NullMarked
public class UiAndroidFeatureList {
    private static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return UiAndroidFeatureMap.getInstance()
                .mutableFlagWithSafeDefault(featureName, defaultValue);
    }

    private static CachedFlag newCachedFlag(
            String featureName, boolean defaultValue, boolean defaultValueInTests) {
        return new CachedFlag(
                UiAndroidFeatureMap.getInstance(), featureName, defaultValue, defaultValueInTests);
    }

    public static final MutableFlagWithSafeDefault sRequireLeadingInTextViewWithLeading =
            newMutableFlagWithSafeDefault(
                    UiAndroidFeatures.REQUIRE_LEADING_IN_TEXT_VIEW_WITH_LEADING, false);

    public static final CachedFlag sAndroidWindowOcclusion =
            newCachedFlag(
                    UiAndroidFeatures.ANDROID_WINDOW_OCCLUSION,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);

    public static final CachedFlag sAndroidWindowManagementWebApi =
            newCachedFlag(
                    UiAndroidFeatures.ANDROID_WINDOW_MANAGEMENT_WEB_API,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);

    public static final List<CachedFlag> sFlagsCachedUiAndroid =
            List.of(sAndroidWindowOcclusion, sAndroidWindowManagementWebApi);
}
