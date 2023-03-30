// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.shell.topbar;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.Spinner;

/**
 * A Spinner wrapper monitoring whether the dialog is open or closed.
 */
public class CustomSpinner extends Spinner {
    private boolean mIsOpen;

    public CustomSpinner(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public boolean performClick() {
        mIsOpen = true;
        return super.performClick();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        if (isOpen() && hasFocus) {
            performClose();
        }
    }

    private void performClose() {
        mIsOpen = false;
    }

    public boolean isOpen() {
        return mIsOpen;
    }
}
