// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.system;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Paint.Style;
import android.graphics.RectF;

import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.SystemUIResourceType;
import org.chromium.ui.resources.async.AsyncPreloadResourceLoader;
import org.chromium.ui.resources.statics.StaticResource;

/** Handles loading system specific resources like overscroll and edge glows. */
public class SystemResourceLoader extends AsyncPreloadResourceLoader {
    private static final float SIN_PI_OVER_6 = 0.5f;
    private static final float COS_PI_OVER_6 = 0.866f;

    /**
     * Creates an instance of a {@link SystemResourceLoader}.
     * @param resourceType The resource type this loader is responsible for loading.
     * @param callback     The {@link ResourceLoaderCallback} to notify when a {@link Resource} is
     *                     done loading.
     * @param minScreenSideLengthPx    The length (in pixels) of the smallest side of the screen.
     */
    public SystemResourceLoader(
            int resourceType, ResourceLoaderCallback callback, final int minScreenSideLengthPx) {
        super(
                resourceType,
                callback,
                new ResourceCreator() {
                    @Override
                    public Resource create(int resId) {
                        return createResource(minScreenSideLengthPx, resId);
                    }
                });
    }

    private static Resource createResource(int minScreenSideLengthPx, int resId) {
        switch (resId) {
            case SystemUIResourceType.OVERSCROLL_GLOW:
                return createOverscrollGlowBitmap(minScreenSideLengthPx);
            default:
                assert false;
        }
        return null;
    }

    private static Resource createOverscrollGlowBitmap(int minScreenSideLengthPx) {
        float arcWidth = minScreenSideLengthPx * 0.5f / SIN_PI_OVER_6;
        float y = COS_PI_OVER_6 * arcWidth;
        float height = arcWidth - y;

        float arcRectX = -arcWidth / 2.f;
        float arcRectY = -arcWidth - y;
        float arcRectWidth = arcWidth * 2.f;
        float arcRectHeight = arcWidth * 2.f;
        RectF arcRect =
                new RectF(arcRectX, arcRectY, arcRectX + arcRectWidth, arcRectY + arcRectHeight);

        Paint arcPaint = new Paint();
        arcPaint.setAntiAlias(true);
        arcPaint.setAlpha(0xBB);
        arcPaint.setStyle(Style.FILL);

        Bitmap bitmap = Bitmap.createBitmap((int) arcWidth, (int) height, Bitmap.Config.ALPHA_8);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawArc(arcRect, 45, 90, true, arcPaint);

        return new StaticResource(bitmap);
    }
}
