// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources;

import android.graphics.Rect;
import android.graphics.RectF;

import org.chromium.ui.resources.statics.NinePatchData;

/** A resource that provides sizing information for layouts. */
public class LayoutResource {
    private final RectF mPadding;
    private final RectF mBitmapSize;
    private final RectF mAperture;

    public LayoutResource(float pxToDp, Resource resource) {
        Rect padding = new Rect();
        Rect aperture = new Rect();
        NinePatchData ninePatchData = resource.getNinePatchData();
        if (ninePatchData != null) {
            padding = ninePatchData.getPadding();
            aperture = ninePatchData.getAperture();
        }
        Rect bitmapSize = resource.getBitmapSize();

        mPadding =
                new RectF(
                        padding.left * pxToDp,
                        padding.top * pxToDp,
                        padding.right * pxToDp,
                        padding.bottom * pxToDp);

        mBitmapSize =
                new RectF(
                        bitmapSize.left * pxToDp,
                        bitmapSize.top * pxToDp,
                        bitmapSize.right * pxToDp,
                        bitmapSize.bottom * pxToDp);

        mAperture =
                new RectF(
                        aperture.left * pxToDp,
                        aperture.top * pxToDp,
                        aperture.right * pxToDp,
                        aperture.bottom * pxToDp);
    }

    /**
     * @return The padded content area of this resource in dp.  For 9-patches this will represent
     *         the valid content of the 9-patch. In all other cases, it will be an empty rect.
     */
    public RectF getPadding() {
        return mPadding;
    }

    /**
     * @return The size of the bitmap in dp;
     */
    public RectF getBitmapSize() {
        return mBitmapSize;
    }

    /**
     * @return The aperture of this resource in dp.  For 9-patches this will represent the area of
     *         the {@link Bitmap} that should not be stretched. In all other cases, it will be an
     *         empty rect.
     */
    public RectF getAperture() {
        return mAperture;
    }
}
