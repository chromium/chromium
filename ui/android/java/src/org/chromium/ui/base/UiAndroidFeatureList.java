// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.chromium.base.MutableFlagWithSafeDefault;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.cached_flags.BooleanCachedFeatureParam;
import org.chromium.components.cached_flags.CachedFeatureParam;
import org.chromium.components.cached_flags.CachedFlag;
import org.chromium.components.cached_flags.IntCachedFeatureParam;
import org.chromium.components.cached_flags.StringCachedFeatureParam;

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

    public static final MutableFlagWithSafeDefault sAndroidUpdateDisplayForContext =
            newMutableFlagWithSafeDefault(
                    UiAndroidFeatures.ANDROID_UPDATE_DISPLAY_FOR_CONTEXT, true);

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

    // Whether to apply optimizations to the window when it is occluded. When false, occlusion
    // metrics will still be collected, but the actual behavior of the window remains unchanged.
    public static final BooleanCachedFeatureParam sAndroidWindowOcclusionOptimizations =
            new BooleanCachedFeatureParam(
                    UiAndroidFeatureMap.getInstance(),
                    UiAndroidFeatures.ANDROID_WINDOW_OCCLUSION,
                    "occlusion_optimizations",
                    false);

    public static final IntCachedFeatureParam
            sAndroidWindowOcclusionMinimumVisibilitySizeThreshold =
                    new IntCachedFeatureParam(
                            UiAndroidFeatureMap.getInstance(),
                            UiAndroidFeatures.ANDROID_WINDOW_OCCLUSION,
                            "minimum_visibility_size_threshold",
                            0);

    public static final StringCachedFeatureParam sAndroidWindowOcclusionTrackingMode =
            new StringCachedFeatureParam(
                    UiAndroidFeatureMap.getInstance(),
                    UiAndroidFeatures.ANDROID_WINDOW_OCCLUSION,
                    "tracking_mode",
                    "self_occlusion");

    public static final CachedFlag sBlockMouseEventsOnView =
            newCachedFlag(
                    UiAndroidFeatures.BLOCK_MOUSE_EVENTS_ON_VIEW,
                    /* defaultValue= */ true,
                    /* defaultValueInTests= */ true);

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

    public static final MutableFlagWithSafeDefault sSupportKeyboard =
            newMutableFlagWithSafeDefault(
                    UiAndroidFeatures.SUPPORT_KEYBOARD, /* defaultValue= */ true);

    public static final List<CachedFlag> sFlagsCachedUiAndroid =
            List.of(
                    sAndroidUseDisplayTopology,
                    sAndroidWindowOcclusion,
                    sRefactorMinWidthContextOverride);

    public static final List<CachedFeatureParam<?>> sParamsCached =
            List.of(
                    // keep-sorted start
                    sAndroidWindowOcclusionMinimumVisibilitySizeThreshold,
                    sAndroidWindowOcclusionOptimizations,
                    sAndroidWindowOcclusionTrackingMode
                    // keep-sorted end
                    );
}
