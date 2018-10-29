// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.text.Layout;
import android.text.style.BulletSpan;

import org.chromium.ui.R;

/**
 * A wrapper around Android's BulletSpan that provides default styling and adjusts the bullet
 * positioning to prevent it from being cut off.
 */
public class ChromeBulletSpan extends BulletSpan {
    private int mXOffset;

    /**
     * Construts a new ChromeBuletSpan.
     * @param context The context of the containing view, used to retrieve dimensions.
     */
    public ChromeBulletSpan(Context context) {
        super(context.getResources().getDimensionPixelSize(R.dimen.chrome_bullet_gap));
        mXOffset =
                context.getResources().getDimensionPixelSize(R.dimen.chrome_bullet_leading_offset);
    }

    @Override
    public void drawLeadingMargin(Canvas c, Paint p, int x, int dir, int top, int baseline,
            int bottom, CharSequence text, int start, int end, boolean first, Layout l) {
        // Android cuts off the bullet points. Adjust the x-position so that the bullets aren't
        // cut off.
        super.drawLeadingMargin(
                c, p, x + mXOffset, dir, top, baseline, bottom, text, start, end, first, l);
    }
}
