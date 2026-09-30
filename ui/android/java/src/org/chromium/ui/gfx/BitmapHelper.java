// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gfx;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Helper class to create and manage Android Bitmaps from native. */
@JNINamespace("gfx")
@NullMarked
public class BitmapHelper {
    private static final String TAG = "BitmapHelper";

    private BitmapHelper() {}

    @CalledByNative
    private static @Nullable Bitmap createBitmap(
            int width,
            int height,
            @JniType("gfx::BitmapFormat") @BitmapFormat int bitmapFormatValue,
            boolean catchOom) {
        Bitmap.Config bitmapConfig = getBitmapConfigForFormat(bitmapFormatValue);
        try {
            return Bitmap.createBitmap(width, height, bitmapConfig);
        } catch (OutOfMemoryError oom) {
            if (!catchOom) throw oom;
            Log.w(TAG, "createBitmap OOM-ed", oom);
            return null;
        }
    }

    /**
     * Provides a matching Bitmap.Config for the enum config value passed.
     *
     * @param bitmapFormatValue The Bitmap Configuration enum value.
     * @return Matching Bitmap.Config for the enum value passed.
     */
    private static Bitmap.Config getBitmapConfigForFormat(@BitmapFormat int bitmapFormatValue) {
        return switch (bitmapFormatValue) {
            case BitmapFormat.ALPHA_8 -> Bitmap.Config.ALPHA_8;
            case BitmapFormat.ARGB_4444 -> Bitmap.Config.ARGB_4444;
            case BitmapFormat.RGB_565 -> Bitmap.Config.RGB_565;
            default -> Bitmap.Config.ARGB_8888;
        };
    }
}
