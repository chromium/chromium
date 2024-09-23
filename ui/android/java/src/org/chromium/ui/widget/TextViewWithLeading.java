// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.util.AttributeSet;

import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;
import androidx.annotation.StyleableRes;
import androidx.appcompat.widget.AppCompatTextView;

import org.chromium.ui.R;
import org.chromium.ui.base.UiAndroidFeatureList;

/**
 * A TextView with the added leading property. Leading is the distance between the baselines of
 * successive lines of text (so the space between rules on ruled paper). This class performs the
 * calculation to set up leading correctly and allows it to be set in XML. It overwrites
 * android:lineSpacingExtra and android:lineSpacingMultiplier.
 */
public class TextViewWithLeading extends AppCompatTextView {
    /**
     * Constructing TextViewWithLeading programmatically without an {@link AttributeSet} will render
     * it functionally equivalent to a TextView - that is no leading will be applied. This method is
     * provided for use from subclasses.
     */
    protected TextViewWithLeading(Context context) {
        super(context);
    }

    /** Constructor for use from XML with checkForLineSpacingAttributes assertion. */
    public TextViewWithLeading(Context context, AttributeSet attrs) {
        super(context, attrs);
        checkForLineSpacingAttributes(attrs);
        Float nullableLeading = getLeadingDimen(attrs);
        // TODO(https://crbug.com:1499069): Remove feature/kill switch once certain this is safe.
        if (UiAndroidFeatureList.sRequireLeadingInTextViewWithLeading.isEnabled()) {
            assert nullableLeading != null : "Couldn't find leading for TextViewWithLeading";
            applyLeading(nullableLeading);
        } else if (nullableLeading != null) {
            applyLeading(nullableLeading);
        }
    }

    private @Nullable Float getLeadingDimen(AttributeSet attrs) {
        // This result variable holds the return value so a single return can be used, allowing
        // easier recycling of TypedArrays.
        final Float result;

        Context context = getContext();
        TypedArray selfTypedArray =
                context.obtainStyledAttributes(attrs, R.styleable.TextViewWithLeading, 0, 0);
        @StyleableRes int leadingIndex = R.styleable.TextViewWithLeading_leading;
        @StyleableRes int textAppIndex = R.styleable.TextViewWithLeading_android_textAppearance;

        if (selfTypedArray.hasValue(leadingIndex)) {
            // Found the attr directly inside the layout or style. This has a higher priority.
            float leading = selfTypedArray.getDimension(leadingIndex, 0f);
            result = leading;
        } else if (selfTypedArray.hasValue(textAppIndex)) {
            // Resolve the text appearance, hopefully the leading is there instead.
            @StyleRes int textAppRes = selfTypedArray.getResourceId(textAppIndex, 0);
            if (textAppRes == Resources.ID_NULL) {
                result = null;
            } else {
                // Using R.styleable.TextViewWithLeading to specify the list of desired attributes,
                // as this is what getLeadingFromTextAppearance will use.
                TypedArray textAppTypedArray =
                        context.obtainStyledAttributes(textAppRes, R.styleable.TextViewWithLeading);
                result = getLeadingFromTextAppearance(textAppTypedArray);
                textAppTypedArray.recycle();
            }
        } else {
            result = null;
        }

        selfTypedArray.recycle();
        return result;
    }

    private @Nullable Float getLeadingFromTextAppearance(TypedArray textAppTypedArray) {
        if (textAppTypedArray.hasValue(R.styleable.TextViewWithLeading_leading)) {
            return textAppTypedArray.getDimension(R.styleable.TextViewWithLeading_leading, 0);
        } else {
            return null;
        }
    }

    private void applyLeading(float newLeading) {
        float oldLeading = getPaint().getFontMetrics(null);
        setLineSpacing(newLeading - oldLeading, 1f);
    }

    private void checkForLineSpacingAttributes(AttributeSet attrs) {
        // TODO(https://crbug.com:1499069): Fix namespaces, this check does not currently work.
        assert attrs.getAttributeValue(null, "android:lineSpacingExtra") == null
                        && attrs.getAttributeValue(null, "android:lineSpacingMultiplier") == null
                : "Do not use android:lineSpacingExtra or android:lineSpacingMultiplier in"
                        + " TextViewWithLeading.";
    }
}
