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

    public static final CachedFlag sAndroidUseCorrectWindowBounds =
            newCachedFlag(
                    UiAndroidFeatures.ANDROID_USE_CORRECT_WINDOW_BOUNDS,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);

    public static final CachedFlag sAndroidUseDisplayTopology =
            newCachedFlag(
                    UiAndroidFeatures.ANDROID_USE_DISPLAY_TOPOLOGY,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ true);

    public static final CachedFlag sAndroidWindowOcclusion =
            newCachedFlag(
                    UiAndroidFeatures.ANDROID_WINDOW_OCCLUSION,
                    /* defaultValue= */ false,
                    /* defaultValueInTests= */ false);

    public static final CachedFlag sRefactorMinWidthContextOverride =
            newCachedFlag(
                    UiAndroidFeatures.REFACTOR_MIN_WIDTH_CONTEXT_OVERRIDE,
                    /* defaultValue= */ true,
                    /* defaultValueInTests= */ true);

    public static final CachedFlag sReportBottomOverscrolls =
            newCachedFlag(
                    UiAndroidFeatures.REPORT_BOTTOM_OVERSCROLLS,
                    /* defaultValue= */ true,
                    /* defaultValueInTests= */ true);

    public static final MutableFlagWithSafeDefault sAndroidTouchpadOverscrollHistoryNavigation =
            // public static final CachedFlag sAndroidTouchpadOverscrollHistoryNavigation =
            // newCachedFlag(UiAndroidFeatures.ANDROID_TOUCHPAD_OVERSCROLL_HISTORY_NAVIGATION,
            newMutableFlagWithSafeDefault(
                    UiAndroidFeatures.ANDROID_TOUCHPAD_OVERSCROLL_HISTORY_NAVIGATION,
                    /* defaultValue= */ true);

    public static final List<CachedFlag> sFlagsCachedUiAndroid =
            List.of(
                    sAndroidUseCorrectWindowBounds,
                    sAndroidUseDisplayTopology,
                    sAndroidWindowOcclusion,
                    sRefactorMinWidthContextOverride);
}
