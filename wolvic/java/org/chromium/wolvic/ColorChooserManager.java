// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wolvic;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

@JNINamespace("wolvic")
public class ColorChooserManager {
    public interface Bridge {
        void show();
        void close();
    }

    public interface Listener {
        void onColorChanged(int color);
    }

    public interface Factory {
        public Bridge create(int initialColor, Listener listener);
    }

    private static Factory sFactory;
    private final Bridge mBridge;
    private final long mNativeColorChooserManager;

    private ColorChooserManager(long nativeColorChooserManager, int initialColor) {
        Listener listener = new Listener() {
            @Override
            public void onColorChanged(int color) {
                ColorChooserManagerJni.get().onColorChosen(
                        mNativeColorChooserManager, ColorChooserManager.this, color);
            }
        };

        mNativeColorChooserManager = nativeColorChooserManager;
        mBridge = sFactory.create(initialColor, listener);
    }

    public static void setFactory(Factory factory) {
        sFactory = factory;
    }

    private void openColorChooser() {
        mBridge.show();
    }

    @CalledByNative
    public void closeColorChooser() {
        mBridge.close();
    }

    @CalledByNative
    public static ColorChooserManager createColorChooser(long nativeColorChooserManager,
            int initialColor) {
        ColorChooserManager chooser = new ColorChooserManager(
                nativeColorChooserManager, initialColor);
        chooser.openColorChooser();
        return chooser;
    }

    @NativeMethods
    interface Natives {
        void onColorChosen(long nativeColorChooserManager, ColorChooserManager caller, int color);
    }
}
