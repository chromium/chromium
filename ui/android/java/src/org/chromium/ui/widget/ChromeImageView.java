// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.util.AttributeSet;

import androidx.appcompat.widget.AppCompatImageView;

// TODO(https://crbug.com/1400720): This class has no use now, so we can get rid of it.
/**
 * A subclass of AppCompatImageView to add workarounds for bugs in Android Framework and Support
 * Library.
 */
public class ChromeImageView extends AppCompatImageView {
    public ChromeImageView(Context context) {
        super(context);
    }

    public ChromeImageView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public ChromeImageView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }
}
