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
import org.chromium.build.annotations.Nullable;

@NullMarked
public class KeyboardAccessibleListView extends ListView {

    private int mSelectedItemPosition = INVALID_POSITION;

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
        @Nullable View focusedChild = getFocusedChild();
        if (focusedChild != null) {
            int position = getPositionForView(focusedChild);
            if (position != INVALID_POSITION) {
                return position;
            }
        }
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
        int count = getCount();
        // If the key event is unmodified tab, or unmodified down arrow,
        if (keyEvent.hasNoModifiers()
                && (keyCode == KeyEvent.KEYCODE_TAB || keyCode == KeyEvent.KEYCODE_DPAD_DOWN)) {
            if (count == 0) {
                searchAndRequestNextFocus(FOCUS_DOWN);
                return true;
            }
            int currentPosition = getSelectedItemPosition();
            int firstVisiblePosition = getFirstVisiblePosition();
            if (currentPosition >= count - 1 && getLastVisiblePosition() >= count - 1) {
                searchAndRequestNextFocus(FOCUS_DOWN);
            } else {
                int targetPosition =
                        MathUtils.clamp(
                                currentPosition < firstVisiblePosition
                                        ? firstVisiblePosition
                                        : currentPosition + 1,
                                0,
                                count - 1);
                selectAndFocusChild(targetPosition, keyCode, keyEvent);
            }
            return true;
        }
        if ((keyEvent.hasModifiers(KeyEvent.META_SHIFT_ON) && keyCode == KeyEvent.KEYCODE_TAB)
                || (keyEvent.hasNoModifiers() && (keyCode == KeyEvent.KEYCODE_DPAD_UP))) {
            if (count == 0) {
                searchAndRequestNextFocus(FOCUS_UP);
                return true;
            }
            int currentPosition = getSelectedItemPosition();
            int firstVisiblePosition = getFirstVisiblePosition();
            boolean hasValidPosition = currentPosition > 0;
            if (!hasValidPosition && firstVisiblePosition <= 0) {
                searchAndRequestNextFocus(FOCUS_UP);
            } else {
                int basePosition = hasValidPosition ? currentPosition : firstVisiblePosition;
                int targetPosition = MathUtils.clamp(basePosition - 1, 0, count - 1);
                selectAndFocusChild(targetPosition, keyCode, keyEvent);
            }
            return true;
        }
        return super.onKeyDown(keyCode, keyEvent);
    }

    private void selectAndFocusChild(int position, int keyCode, KeyEvent keyEvent) {
        setSelection(position);
        super.onKeyDown(keyCode, keyEvent);
        focusChildAtPosition(position);
    }

    private void focusChildAtPosition(int position) {
        int childIndex = position - getFirstVisiblePosition();
        if (childIndex >= 0 && childIndex < getChildCount()) {
            @Nullable View child = getChildAt(childIndex);
            if (child != null) {
                child.requestFocus();
            }
        }
    }

    /**
     * Searches for the next focusable view in the specified direction and requests focus on it if
     * found. Searches from the focused child if one exists, otherwise falls back to searching from
     * this {@link ListView} to avoid {@link NullPointerException}.
     *
     * @param direction The direction in which to search for focus (e.g. {@link View#FOCUS_DOWN} or
     *     {@link View#FOCUS_UP}).
     */
    private void searchAndRequestNextFocus(int direction) {
        // If a child currently has focus, search from that child; otherwise search from
        // the ListView container itself. If getFocusedChild() is null, calling focusSearch
        // on it directly would throw a NullPointerException.
        @Nullable View focusedChild = getFocusedChild();
        @Nullable View nextView =
                focusedChild != null ? focusedChild.focusSearch(direction) : focusSearch(direction);
        // We focus the next item when there is a next item. If there's no next item, stop.
        if (nextView != null) {
            nextView.requestFocus();
        }
    }
}
