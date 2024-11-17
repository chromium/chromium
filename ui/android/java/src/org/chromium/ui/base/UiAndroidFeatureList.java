// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.chromium.base.MutableFlagWithSafeDefault;

/** Helpers and state for features from {@link UiAndroidFeatures}. */
public class UiAndroidFeatureList {
    private static MutableFlagWithSafeDefault newMutableFlagWithSafeDefault(
            String featureName, boolean defaultValue) {
        return UiAndroidFeatureMap.getInstance()
                .mutableFlagWithSafeDefault(featureName, defaultValue);
    }

    public static final MutableFlagWithSafeDefault sRequireLeadingInTextViewWithLeading =
            newMutableFlagWithSafeDefault(
                    UiAndroidFeatures.REQUIRE_LEADING_IN_TEXT_VIEW_WITH_LEADING, false);

    public static final String DRAG_DROP_EMPTY = "DragDropEmpty";
    public static final String DRAG_DROP_FILES = "DragDropFiles";
}
