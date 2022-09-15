// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;

/**
 * Draws a horizontal list divider line at the bottom of its drawing area.
 *
 * Because ?android:attr/listDivider may be a 9-patch, there's no way to achieve this drawing
 * effect with the platform Drawable classes; hence this custom Drawable.
 */
public class HorizontalListDividerDrawable extends LayerDrawable {
    /**
     * Create a horizontal list divider drawable.
     *
     * @param context The context used to create drawable.
     * @return The drawable.
     */
    public static HorizontalListDividerDrawable create(Context context) {
        TypedArray a = context.obtainStyledAttributes(new int[] {android.R.attr.listDivider});
        Drawable listDivider = a.getDrawable(0);
        a.recycle();
        return new HorizontalListDividerDrawable(new Drawable[] {listDivider});
    }

    private HorizontalListDividerDrawable(Drawable[] layers) {
        super(layers);
    }

    @Override
    protected void onBoundsChange(Rect bounds) {
        int listDividerHeight = getDrawable(0).getIntrinsicHeight();
        setLayerInset(0, 0, bounds.height() - listDividerHeight, 0, 0);
        super.onBoundsChange(bounds);
    }
}
