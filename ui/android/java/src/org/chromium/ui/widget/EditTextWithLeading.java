// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.util.AttributeSet;

import androidx.appcompat.widget.AppCompatEditText;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * An EditText with the added leading property. Leading is the distance between the baselines of
 * successive lines of text (so the space between rules on ruled paper). Logical implementation
 * lives in {@link TextViewLeadingUtils} for sharing with {@link TextViewWithLeading}
 */
@NullMarked
public class EditTextWithLeading extends AppCompatEditText {

    /**
     * If the {@link AttributeSet} is not provided, this will be functionally equivalent to a
     * TextView - that is no leading will be applied. This method is also available for use from
     * subclasses besides inflation from XML.
     */
    public EditTextWithLeading(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        TextViewLeadingUtils.applySpacingAttributes(this, attrs, context);
    }
}
