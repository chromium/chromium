// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.widget.ListView;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;

@NullMarked
public class KeyboardAccessibleListView extends ListView {

    private int mSelectedItemPosition;

    public KeyboardAccessibleListView(Context context) {
        super(context);
    }

    public KeyboardAccessibleListView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public KeyboardAccessibleListView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public KeyboardAccessibleListView(
            Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
    }

    @Override
    public int getSelectedItemPosition() {
        return mSelectedItemPosition;
    }

    @Override
    public void setSelection(int i) {
        mSelectedItemPosition = i;
        super.setSelection(i);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent keyEvent) {
        if (keyEvent.getAction() != KeyEvent.ACTION_DOWN) {
            return super.onKeyDown(keyCode, keyEvent);
        }
        // If the key event is unmodified tab, or unmodified down arrow,
        if (keyEvent.hasNoModifiers()
                && (keyCode == KeyEvent.KEYCODE_TAB || keyCode == KeyEvent.KEYCODE_DPAD_DOWN)) {
            if (mSelectedItemPosition >= getCount() - 1) {
                View nextView = getFocusedChild().focusSearch(FOCUS_DOWN);
                // We focus the next item when there is a next item. If there's no next item, stop.
                if (nextView != null) {
                    nextView.requestFocus();
                }
            } else {
                mSelectedItemPosition =
                        MathUtils.clamp(mSelectedItemPosition, 0, getCount() - 1) + 1;
                super.onKeyDown(keyCode, keyEvent);
            }
            return true;
        }
        if ((keyEvent.hasModifiers(KeyEvent.META_SHIFT_ON) && keyCode == KeyEvent.KEYCODE_TAB)
                || (keyEvent.hasNoModifiers() && (keyCode == KeyEvent.KEYCODE_DPAD_UP))) {
            if (mSelectedItemPosition <= 0) {
                View nextView = getFocusedChild().focusSearch(FOCUS_UP);
                // We focus the next item when there is a next item. If there's no next item, stop.
                if (nextView != null) {
                    nextView.requestFocus();
                }
            } else {
                mSelectedItemPosition =
                        MathUtils.clamp(mSelectedItemPosition, 0, getCount() - 1) - 1;
                super.onKeyDown(keyCode, keyEvent);
            }
            return true;
        }
        return super.onKeyDown(keyCode, keyEvent);
    }
}
