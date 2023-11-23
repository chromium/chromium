// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.view.View;

/** Shared stateless capture utility functions. */
public class CaptureUtils {
    /** Creates a bitmap with the given size. */
    public static Bitmap createBitmap(int width, int height) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        bitmap.setHasAlpha(true);
        return bitmap;
    }

    /**
     * Called to draw the {@link View}'s contents into the passed in {@link Canvas}.
     *
     * @param canvas The {@link Canvas} that will be drawn to.
     * @param view The view being captuerd.
     * @param dirtyRect The area of the view that might have changed.
     * @param scale The scale to capture the view at.
     * @param drawWhileDetached drawing while detached causes crashes for both software and hardware
     *     renderer, since enabling hardware renderer caused a regression in number of crashes, this
     *     boolean will only be true for software renderer, and will be removed later on if the
     *     issue was fixed for the hardware renderer and logic for avoiding the draw would be the
     *     same for both hardware and software renderer. Software or hardware draw will both need to
     *     follow this pattern.
     * @param observer Should be notified before and after the actual bitmap draw happens.
     * @return true if the draw is successful, false if we couldn't draw because the view is
     *     detached.
     */
    public static boolean captureCommon(
            Canvas canvas,
            View view,
            Rect dirtyRect,
            float scale,
            boolean drawWhileDetached,
            CaptureObserver observer) {
        boolean willDraw = drawWhileDetached || view.isAttachedToWindow();
        if (!willDraw) {
            return false;
        }
        observer.onCaptureStart(canvas, dirtyRect.isEmpty() ? null : dirtyRect);
        if (!dirtyRect.isEmpty()) {
            canvas.clipRect(dirtyRect);
        }

        canvas.save();
        canvas.scale(scale, scale);
        view.draw(canvas);
        canvas.restore();

        observer.onCaptureEnd();
        return true;
    }
}
