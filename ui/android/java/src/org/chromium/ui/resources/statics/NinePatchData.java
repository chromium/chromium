// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.statics;

import android.graphics.Bitmap;
import android.graphics.NinePatch;
import android.graphics.Rect;

import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/** A helper class to decode and expose relevant 9-patch data from a Bitmap. */
public class NinePatchData {
    private final int mWidth;
    private final int mHeight;
    private final Rect mPadding;
    private final int[] mDivX;
    private final int[] mDivY;

    private Rect mAperture;

    /**
     * Creates a {@link NinePatchData} that stores 9-patch metadata.
     * @param width   The width of the underlying bitmap.
     * @param height  The height of the underlying bitmap.
     * @param padding The padding of the 9-patch for the content area.  This padding is a set of
     *                insets (left = left padding, top = top padding, right = right padding,
     *                bottom = bottom padding).
     * @param divX    A run-length encoded list of stretch regions along the x dimension.  The
     *                regions will go from 0 -> divX[0] - 1, divX[0] -> divX[1] - 1, etc..
     * @param divY    A run-length encoded list of stretch regions along the y dimension.  The
     *                regions will go from 0 -> divY[0] - 1, divY[0] -> divY[1] - 1, etc..
     */
    private NinePatchData(int width, int height, Rect padding, int[] divX, int[] divY) {
        mWidth = width;
        mHeight = height;
        mPadding =
                new Rect(
                        padding.left,
                        padding.top,
                        mWidth - padding.right,
                        mHeight - padding.bottom);

        mDivX = new int[divX.length];
        mDivY = new int[divY.length];

        System.arraycopy(divX, 0, mDivX, 0, divX.length);
        System.arraycopy(divY, 0, mDivY, 0, divY.length);

        mAperture = new Rect(mDivX[0], mDivY[0], mDivX[1], mDivY[1]);
    }

    /**
     * @return The padded content area of this 9-patch.
     */
    public Rect getPadding() {
        return mPadding;
    }

    /**
     * This class only exposes one 9-patch stretch region.
     * @return The aperture of this 9-patch.  This specifies the center of the 9-patch, with the
     *         surrounding areas being stretchable.
     */
    public Rect getAperture() {
        return mAperture;
    }

    /**
     * Attempts to decode 9-patch data from a {@link Bitmap}.
     * @param bitmap The {@link Bitmap} to check.
     * @return       An instance of {@link NinePatchData} representing the 9-patch information
     *               encoded in {@code bitmap} or {@code null} if the {@link Bitmap} wasn't a
     *               9-patch.
     */
    public static NinePatchData create(Bitmap bitmap) {
        if (bitmap == null) return null;

        try {
            byte[] chunk = bitmap.getNinePatchChunk();
            if (chunk == null || !NinePatch.isNinePatchChunk(chunk)) return null;

            ByteBuffer buffer = ByteBuffer.wrap(chunk).order(ByteOrder.nativeOrder());

            // int8_t wasDeserialized
            if (buffer.get() == 0) return null;

            // int8_t numXDivs
            int numDivX = buffer.get();
            if (numDivX == 0 || (numDivX & 0x01) != 0) return null;

            // int8_t numYDivs
            int numDivY = buffer.get();
            if (numDivY == 0 || (numDivY & 0x01) != 0) return null;

            // int8_t numColors
            buffer.get();

            // uint32_t xDivsOffset
            buffer.getInt();

            // uint32_t yDivsOffset
            buffer.getInt();

            Rect padding = new Rect();

            // uint32_t paddingLeft
            padding.left = buffer.getInt();

            // uint32_t paddingRight
            padding.right = buffer.getInt();

            // uint32_t paddingTop
            padding.top = buffer.getInt();

            // uint32_t paddingBottom
            padding.bottom = buffer.getInt();

            // uint32_t colorsOffset
            buffer.getInt();

            // uint32_t uint32_t uint32_t ...
            int[] divX = new int[numDivX];
            for (int i = 0; i < numDivX; i++) divX[i] = buffer.getInt();

            // uint32_t uint32_t uint32_t ...
            int[] divY = new int[numDivY];
            for (int i = 0; i < numDivY; i++) divY[i] = buffer.getInt();

            return new NinePatchData(bitmap.getWidth(), bitmap.getHeight(), padding, divX, divY);
        } catch (BufferUnderflowException ex) {
            return null;
        }
    }
}
