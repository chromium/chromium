// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.base.KeyNavigationUtil.isActionDown;
import static org.chromium.ui.base.KeyNavigationUtil.isGoDown;
import static org.chromium.ui.base.KeyNavigationUtil.isGoUp;
import static org.chromium.ui.base.KeyNavigationUtil.isTabBackward;
import static org.chromium.ui.base.KeyNavigationUtil.isTabForward;

import android.content.Context;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.ListAdapter;
import android.widget.ListView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.accessibility.AccessibilityState;

import java.util.ArrayList;

@NullMarked
public class KeyboardAccessibleListView extends ListView {
    private static final String EXTRAS_KEY_CHROME_ROLE = "AccessibilityNodeInfo.chromeRole";
    private static final String CHROME_ROLE_MENU = "menu";

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
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);
        if (getCount() > 0 && AccessibilityState.isKnownScreenReaderEnabled()) {
            // Expose chromeRole="menu" when a known screen reader is active so TalkBack's Smart
            // Browse Mode recognizes this ListView as an arrow-key-navigable menu and passes arrow
            // keys through instead of consuming them as Browse Mode reading commands. Note that
            // because TalkBack's FocusActorHelper#shouldSyncInputFocusToAccessibilityFocus checks
            // ancestors for chromeRole="menu" during keyboard navigation (INPUT_MODE_KEYBOARD),
            // navigating menu items with TalkBack+Left/Right (Search+Left/Right) will also sync
            // keyboard focus onto focusable menu items, whereas touch left/right swipes
            // (INPUT_MODE_TOUCH) only move TalkBack accessibility focus.
            info.getExtras().putCharSequence(EXTRAS_KEY_CHROME_ROLE, CHROME_ROLE_MENU);
        } else {
            info.getExtras().remove(EXTRAS_KEY_CHROME_ROLE);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        // When a PopupWindow opens in touch mode, PopupDecorView does not automatically focus its
        // content view or emit TYPE_VIEW_FOCUSED. TalkBack's Smart Browse Mode only checks
        // chromeRole="menu" to exit Browse Mode upon receiving TYPE_VIEW_FOCUSED, and ignores
        // TYPE_VIEW_FOCUSED on Role.ROLE_LIST containers for accessibility focus syncing.
        if (hasWindowFocus
                && getCount() > 0
                && getDescendantFocusability() != FOCUS_BLOCK_DESCENDANTS) {
            if (!hasFocus()) {
                // Neither this ListView nor any descendant child has focus. Focusing the ListView
                // triggers View#onFocusChanged(true), which emits TYPE_VIEW_FOCUSED.
                requestFocus();
            } else if (isFocused()) {
                // If the ListView container itself was focused before the window gained focus (as
                // opposed to a descendant child being focused, where hasFocus() is true and
                // isFocused() is false), emit TYPE_VIEW_FOCUSED now that the window is focused.
                // When a descendant child is already focused, do nothing so child focus is
                // preserved.
                sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
            }
        }
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
        if (!isActionDown(keyEvent)) {
            return super.onKeyDown(keyCode, keyEvent);
        }
        int count = getCount();
        int currentPosition = getSelectedItemPosition();
        int firstVisiblePosition = getFirstVisiblePosition();
        // If the key event is unmodified tab, or unmodified down arrow,
        if (isTabForward(keyEvent) || (keyEvent.hasNoModifiers() && isGoDown(keyEvent))) {
            if (count == 0) {
                searchAndRequestNextFocus(FOCUS_DOWN);
                return true;
            }
            if (isTabForward(keyEvent) && focusNextDescendantInCurrentRow(/* forward= */ true)) {
                return true;
            }
            int startPosition =
                    currentPosition < firstVisiblePosition
                            ? firstVisiblePosition
                            : currentPosition + 1;
            int targetPosition = findFocusablePosition(startPosition, /* lookDown= */ true);
            if (targetPosition == INVALID_POSITION && getLastVisiblePosition() >= count - 1) {
                searchAndRequestNextFocus(FOCUS_DOWN);
            } else if (targetPosition != INVALID_POSITION) {
                selectAndFocusChild(targetPosition, keyCode, keyEvent);
            } else if (!super.onKeyDown(keyCode, keyEvent)) {
                // Delegate to ListView so it can scroll further down when not yet at the bottom;
                // if ListView cannot scroll further (e.g. trailing off-screen disabled rows),
                // fall back to searching for focus outside the list.
                searchAndRequestNextFocus(FOCUS_DOWN);
            }
            return true;
        }
        if (isTabBackward(keyEvent) || (keyEvent.hasNoModifiers() && isGoUp(keyEvent))) {
            if (count == 0) {
                searchAndRequestNextFocus(FOCUS_UP);
                return true;
            }
            if (isTabBackward(keyEvent) && focusNextDescendantInCurrentRow(/* forward= */ false)) {
                return true;
            }
            int startPosition =
                    currentPosition > 0 ? currentPosition - 1 : firstVisiblePosition - 1;
            int targetPosition = findFocusablePosition(startPosition, /* lookDown= */ false);
            if (targetPosition == INVALID_POSITION && firstVisiblePosition <= 0) {
                searchAndRequestNextFocus(FOCUS_UP);
            } else if (targetPosition != INVALID_POSITION) {
                selectAndFocusChild(targetPosition, keyCode, keyEvent);
            } else if (!super.onKeyDown(keyCode, keyEvent)) {
                // Delegate to ListView so it can scroll further up when not yet at the top;
                // if ListView cannot scroll further (e.g. leading off-screen disabled rows),
                // fall back to searching for focus outside the list.
                searchAndRequestNextFocus(FOCUS_UP);
            }
            return true;
        }
        return super.onKeyDown(keyCode, keyEvent);
    }

    /**
     * Moves focus to the next or previous focusable descendant within the currently focused row if
     * the row contains multiple focusable views (e.g. a Share row with an inline quick-share
     * button). Returns {@code true} if focus moved within the current row, or {@code false} if
     * focus should advance to another row.
     */
    private boolean focusNextDescendantInCurrentRow(boolean forward) {
        @Nullable View focusedChild = getFocusedChild();
        if (focusedChild == null) return false;
        @Nullable View focusedView = focusedChild.findFocus();
        if (focusedView == null) return false;
        ArrayList<View> focusables =
                focusedChild.getFocusables(forward ? FOCUS_FORWARD : FOCUS_BACKWARD);
        int nextIndex =
                forward
                        ? focusables.lastIndexOf(focusedView) + 1
                        : focusables.indexOf(focusedView) - 1;
        if (nextIndex >= 0 && nextIndex < focusables.size()) {
            return focusables.get(nextIndex).requestFocus();
        }
        return false;
    }

    /**
     * Finds the next focusable adapter position starting at {@code startPosition} in the given
     * direction. Focusable positions are enabled items, which ignores headers and section dividers.
     *
     * <p>Returns {@link #INVALID_POSITION} if no enabled item exists in that direction.
     *
     * @param startPosition The adapter position to start searching from (inclusive).
     * @param lookDown Whether to search downward (increasing index) or upward (decreasing index).
     * @return The next focusable adapter position, or {@link #INVALID_POSITION} if none exists.
     */
    private int findFocusablePosition(int startPosition, boolean lookDown) {
        @Nullable ListAdapter adapter = getAdapter();
        if (adapter == null) return INVALID_POSITION;
        if (lookDown) {
            for (int i = startPosition; i < adapter.getCount(); i++) {
                if (adapter.isEnabled(i)) return i;
            }
        } else {
            for (int i = Math.min(adapter.getCount() - 1, startPosition); i >= 0; i--) {
                if (adapter.isEnabled(i)) return i;
            }
        }
        return INVALID_POSITION;
    }

    private void selectAndFocusChild(int position, int keyCode, KeyEvent keyEvent) {
        setSelection(position);
        super.onKeyDown(keyCode, keyEvent);
        // When setItemsCanFocus(true) is set, ListView#setSelection does not automatically move
        // input focus to the selected child view, so explicitly focus the child if visible.
        // Pass FOCUS_BACKWARD on Shift+Tab so a row with multiple focusable descendants focuses its
        // last focusable view first when tabbing backward; pass FOCUS_FORWARD otherwise so the
        // row's primary (first) focusable view receives focus.
        int direction = isTabBackward(keyEvent) ? FOCUS_BACKWARD : FOCUS_FORWARD;
        focusChildAtPosition(position, direction);
    }

    private void focusChildAtPosition(int position, int direction) {
        int childIndex = position - getFirstVisiblePosition();
        if (childIndex >= 0 && childIndex < getChildCount()) {
            @Nullable View child = getChildAt(childIndex);
            if (child != null) {
                child.requestFocus(direction);
            }
        } else {
            // The target position is currently off-screen; setSelection(position) schedules an
            // asynchronous layout pass to scroll it into view. Clear focus from the previously
            // focused child while temporarily blocking descendant focus so
            // getSelectedItemPosition()
            // falls back to mSelectedItemPosition instead of returning the old child's position,
            // and post focusing the new child once layout completes.
            @Nullable View focusedChild = getFocusedChild();
            if (focusedChild != null) {
                int descendantFocusability = getDescendantFocusability();
                setDescendantFocusability(FOCUS_BLOCK_DESCENDANTS);
                focusedChild.clearFocus();
                setDescendantFocusability(descendantFocusability);
            }
            post(
                    () -> {
                        if (mSelectedItemPosition == position) {
                            int index = position - getFirstVisiblePosition();
                            if (index >= 0 && index < getChildCount()) {
                                @Nullable View child = getChildAt(index);
                                if (child != null) {
                                    child.requestFocus(direction);
                                }
                            }
                        }
                    });
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
