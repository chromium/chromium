// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for ui/accessibility/ui_accessibility_feature_map.cc state */
@JNINamespace("ui")
public class UiAccessibilityFeatureMap extends FeatureMap {
    private static final UiAccessibilityFeatureMap sInstance = new UiAccessibilityFeatureMap();

    // Do not instantiate this class
    private UiAccessibilityFeatureMap() {}

    /**
     * @return the singleton UiAccessibilityFeatureMap.
     */
    public static UiAccessibilityFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return UiAccessibilityFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
