// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for ui/accessibility/accessibility_features.cc state */
@JNINamespace("ui")
public class AccessibilityFeaturesMap extends FeatureMap {
    private static final AccessibilityFeaturesMap sInstance = new AccessibilityFeaturesMap();

    // Do not instantiate this class
    private AccessibilityFeaturesMap() {}

    /**
     * @return the singleton AccessibilityFeaturesMap.
     */
    public static AccessibilityFeaturesMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return AccessibilityFeaturesMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
