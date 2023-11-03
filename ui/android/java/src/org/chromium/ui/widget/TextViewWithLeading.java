// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.widget.TextView;

import org.chromium.ui.R;

/**
 * A TextView with the added leading property. Leading is the distance between the baselines of
 * successive lines of text (so the space between rules on ruled paper). This class performs the
 * calculation to set up leading correctly and allows it to be set in XML. It overwrites
 * android:lineSpacingExtra and android:lineSpacingMultiplier.
 */
public class TextViewWithLeading extends TextView {
    /**
     * Constructing TextViewWithLeading programmatically without an {@link AttributeSet} will
     * render it functionally equivalent to a TextView - that is no leading will be applied. This
     * method is provided for use from subclasses.
     */
    protected TextViewWithLeading(Context context) {
        super(context);
    }

    /** Constructor for use from XML with checkForLineSpacingAttributes assertion. */
    public TextViewWithLeading(Context context, AttributeSet attrs) {
        super(context, attrs);
        checkForLineSpacingAttributes(attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.TextViewWithLeading, 0, 0);
        if (a.hasValue(R.styleable.TextViewWithLeading_leading)) {
            final float leading = a.getDimension(R.styleable.TextViewWithLeading_leading, 0f);
            final float oldLeading = getPaint().getFontMetrics(null);
            setLineSpacing(leading - oldLeading, 1f);
        }

        a.recycle();
    }

    private void checkForLineSpacingAttributes(AttributeSet attrs) {
        assert attrs.getAttributeValue(null, "android:lineSpacingExtra") == null
                        && attrs.getAttributeValue(null, "android:lineSpacingMultiplier") == null
                : "Do not use android:lineSpacingExtra or android:lineSpacingMultiplier in"
                        + " TextViewWithLeading.";
    }
}
