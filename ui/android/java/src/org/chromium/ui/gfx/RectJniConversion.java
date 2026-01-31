// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gfx;

import android.graphics.Rect;
import android.graphics.RectF;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

/** Helper class to convert to and from android.graphics.Rect. */
@JNINamespace("gfx")
@NullMarked
class RectJniConversion {
    @CalledByNative
    private static Rect createRect(int x, int y, int right, int bottom) {
        return new Rect(x, y, right, bottom);
    }

    @CalledByNative
    private static int getX(Rect rect) {
        return rect.left;
    }

    @CalledByNative
    private static int getY(Rect rect) {
        return rect.top;
    }

    @CalledByNative
    private static int getWidth(Rect rect) {
        return rect.width();
    }

    @CalledByNative
    private static int getHeight(Rect rect) {
        return rect.height();
    }

    @CalledByNative
    private static RectF createRectF(int x, int y, int right, int bottom) {
        return new RectF(x, y, right, bottom);
    }

    @CalledByNative
    private static float getXF(RectF rect) {
        return rect.left;
    }

    @CalledByNative
    private static float getYF(RectF rect) {
        return rect.top;
    }

    @CalledByNative
    private static float getWidthF(RectF rect) {
        return rect.width();
    }

    @CalledByNative
    private static float getHeightF(RectF rect) {
        return rect.height();
    }
}
