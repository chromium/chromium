// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.graphics.PorterDuff.Mode;
import android.util.AttributeSet;

import androidx.appcompat.widget.AppCompatImageButton;

// TODO(crbug.com/40883889): See if we still need this class.
/**
 * A subclass of AppCompatImageButton to add workarounds for bugs in Android Framework and Support
 * Library.
 */
public class ChromeImageButton extends AppCompatImageButton {
    public ChromeImageButton(Context context) {
        super(context);
    }

    public ChromeImageButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public ChromeImageButton(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        // ImageView defaults to SRC_ATOP when there's a tint. This interacts poorly with tints that
        // contain alpha, so adjust the default to SRC_IN when this case is found. This will cause
        // the drawable to be mutated, but the tint should already be causing that anyway.
        if (getImageTintList() != null && getImageTintMode() == Mode.SRC_ATOP) {
            setImageTintMode(Mode.SRC_IN);
        }
    }
}
