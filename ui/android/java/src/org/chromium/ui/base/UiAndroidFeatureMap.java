// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.chromium.base.FeatureMap;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java accessor for ui/android/ui_android_feature_map.cc state
 */
@JNINamespace("ui")
public class UiAndroidFeatureMap extends FeatureMap {
    private static UiAndroidFeatureMap sInstance;

    // Do not instantiate this class
    private UiAndroidFeatureMap() {}

    /**
     * @return the singleton UiAndroidFeatureMap.
     */
    public static UiAndroidFeatureMap getInstance() {
        if (sInstance == null) sInstance = new UiAndroidFeatureMap();
        return sInstance;
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
