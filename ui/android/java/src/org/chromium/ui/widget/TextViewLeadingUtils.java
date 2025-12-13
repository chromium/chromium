// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.widget.TextView;

import androidx.annotation.StyleRes;
import androidx.annotation.StyleableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.UiAndroidFeatureList;

/**
 * Utils for TextView subclasses with the added leading property. Since EditText and TextView both
 * inherit from TextView, this class can be used by both without code duplication.
 *
 * <p>Leading is the distance between the baselines of successive lines of text (so the space
 * between rules on ruled paper). This class performs the calculation to set up leading correctly
 * and allows it to be set in XML. It overwrites android:lineSpacingExtra and
 * android:lineSpacingMultiplier.
 */
@NullMarked
public class TextViewLeadingUtils {

    private TextViewLeadingUtils() {}

    static void applySpacingAttributes(
            TextView textView, @Nullable AttributeSet attrs, Context context) {
        if (attrs == null) return;
        checkForLineSpacingAttributes(attrs);
        Float nullableLeading = getLeadingDimen(attrs, context);
        // TODO(crbug.com/40287683): Remove feature/kill switch once certain this is safe.
        if (UiAndroidFeatureList.sRequireLeadingInTextViewWithLeading.isEnabled()) {
            assert nullableLeading != null : "Couldn't find leading for TextViewWithLeading";
            applyLeading(textView, nullableLeading);
        } else if (nullableLeading != null) {
            applyLeading(textView, nullableLeading);
        }
    }

    static @Nullable Float getLeadingDimen(AttributeSet attrs, Context context) {
        // This result variable holds the return value so a single return can be used, allowing
        // easier recycling of TypedArrays.
        final Float result;

        TypedArray selfTypedArray =
                context.obtainStyledAttributes(
                        attrs, org.chromium.ui.R.styleable.TextViewWithLeading, 0, 0);
        @StyleableRes int leadingIndex = org.chromium.ui.R.styleable.TextViewWithLeading_leading;
        @StyleableRes
        int textAppIndex = org.chromium.ui.R.styleable.TextViewWithLeading_android_textAppearance;

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
                        context.obtainStyledAttributes(
                                textAppRes, org.chromium.ui.R.styleable.TextViewWithLeading);
                result = getLeadingFromTextAppearance(textAppTypedArray);
                textAppTypedArray.recycle();
            }
        } else {
            result = null;
        }

        selfTypedArray.recycle();
        return result;
    }

    static @Nullable Float getLeadingFromTextAppearance(TypedArray textAppTypedArray) {
        if (textAppTypedArray.hasValue(org.chromium.ui.R.styleable.TextViewWithLeading_leading)) {
            return textAppTypedArray.getDimension(
                    org.chromium.ui.R.styleable.TextViewWithLeading_leading, 0);
        } else {
            return null;
        }
    }

    static void applyLeading(TextView textView, float newLeading) {
        float oldLeading = textView.getPaint().getFontMetrics(null);
        textView.setLineSpacing(newLeading - oldLeading, 1f);
    }

    static void checkForLineSpacingAttributes(AttributeSet attrs) {
        // TODO(https://crbug.com:1499069): Fix namespaces, this check does not currently work.
        assert attrs.getAttributeValue(null, "android:lineSpacingExtra") == null
                        && attrs.getAttributeValue(null, "android:lineSpacingMultiplier") == null
                : "Do not use android:lineSpacingExtra or android:lineSpacingMultiplier in"
                        + " TextViewWithLeading.";
    }
}
