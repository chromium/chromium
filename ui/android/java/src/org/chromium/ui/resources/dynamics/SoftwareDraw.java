// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;

/** Simple bitmap capture approach simply calling {@link View#draw(Canvas)}. */
public class SoftwareDraw implements ViewResourceAdapter.CaptureMechanism {
    private Bitmap mBitmap;

    @Override
    public boolean shouldRemoveResourceOnNullBitmap() {
        return false;
    }

    @Override
    public void onViewSizeChange(View view, float scale) {}

    @Override
    public void dropCachedBitmap() {
        mBitmap = null;
    }

    @Override
    public boolean startBitmapCapture(
            View view,
            Rect dirtyRect,
            float scale,
            CaptureObserver observer,
            Callback<Bitmap> onBitmapCapture) {
        try (TraceEvent e = TraceEvent.scoped("SoftwareDraw:syncCaptureBitmap")) {
            int scaledWidth = (int) (view.getWidth() * scale);
            int scaledHeight = (int) (view.getHeight() * scale);

            boolean isEmpty = scaledWidth == 0 || scaledHeight == 0;
            if (isEmpty) {
                scaledWidth = 1;
                scaledHeight = 1;
            }

            if (mBitmap != null
                    && (mBitmap.getWidth() != scaledWidth || mBitmap.getHeight() != scaledHeight)) {
                mBitmap.recycle();
                mBitmap = null;
            }
            if (mBitmap == null) {
                mBitmap = CaptureUtils.createBitmap(scaledWidth, scaledHeight);
            }

            if (!isEmpty) {
                Canvas canvas = new Canvas(mBitmap);
                CaptureUtils.captureCommon(
                        canvas, view, dirtyRect, scale, /* drawWhileDetached= */ true, observer);
            } else {
                assert mBitmap.getWidth() == 1 && mBitmap.getHeight() == 1;
                mBitmap.setPixel(0, 0, Color.TRANSPARENT);
            }

            onBitmapCapture.onResult(mBitmap);
            return !isEmpty;
        }
    }
}
