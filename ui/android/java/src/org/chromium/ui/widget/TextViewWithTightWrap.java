// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.text.Layout;
import android.util.AttributeSet;
import android.widget.TextView;

/**
 * A TextView with an accurate width calculation support for multilines.
 * This is used when we want no extra padding on a multiline TextView. The class perform the width
 * calculation in the overwritten OnMeasure() method.
 */
public class TextViewWithTightWrap extends TextView {
    /** Constructing TextViewWithTightWrap programmatically is similar to a normal TextView. */
    public TextViewWithTightWrap(Context context) {
        super(context);
    }

    /** Constructor for use from XML. */
    public TextViewWithTightWrap(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /** Constructor for use from XML. */
    public TextViewWithTightWrap(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        Layout layout = getLayout();
        if (layout != null && layout.getLineCount() > 1) {
            int width =
                    getMaxLineWidth(layout) + getCompoundPaddingLeft() + getCompoundPaddingRight();
            if (width < getMeasuredWidth()) {
                super.onMeasure(
                        MeasureSpec.makeMeasureSpec(width, MeasureSpec.AT_MOST), heightMeasureSpec);
            }
        }
    }

    private int getMaxLineWidth(Layout layout) {
        float maxWidth = 0;
        int lines = layout.getLineCount();
        for (int i = 0; i < lines; i++) {
            maxWidth = Math.max(maxWidth, layout.getLineWidth(i));
        }
        return (int) Math.ceil(maxWidth);
    }
}
