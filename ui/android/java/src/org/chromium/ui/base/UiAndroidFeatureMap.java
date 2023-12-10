// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for ui/android/ui_android_feature_map.cc state */
@JNINamespace("ui")
public class UiAndroidFeatureMap extends FeatureMap {
    private static final UiAndroidFeatureMap sInstance = new UiAndroidFeatureMap();

    // Do not instantiate this class
    private UiAndroidFeatureMap() {}

    /**
     * @return the singleton UiAndroidFeatureMap.
     */
    public static UiAndroidFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return UiAndroidFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
