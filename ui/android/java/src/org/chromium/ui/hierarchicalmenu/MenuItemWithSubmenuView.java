// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.hierarchicalmenu;

import static android.view.accessibility.AccessibilityNodeInfo.EXPANDED_STATE_COLLAPSED;

import static org.chromium.ui.base.KeyNavigationUtil.isGoForward;

import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * View for a menu item with submenu (of types of e.g. {@code
 * AppMenuItemType.MENU_ITEM_WITH_SUBMENU}, {@code ListItemType.MENU_ITEM_WITH_SUBMENU}).
 */
@NullMarked
class MenuItemWithSubmenuView extends LinearLayout {

    public MenuItemWithSubmenuView(Context context) {
        super(context);
    }

    public MenuItemWithSubmenuView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public MenuItemWithSubmenuView(
            Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public MenuItemWithSubmenuView(
            Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
    }

    @Override
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo info) {
        super.onInitializeAccessibilityNodeInfo(info);
        info.addAction(AccessibilityAction.ACTION_EXPAND);
        // EXPANDED_STATE_COLLAPSED only exists in >= 36. Sadly, the EXPANDED_STATE_COLLAPSED
        // constant is private in AccessibilityNodeInfoCompat (possibly by mistake?). We filed a bug
        // and hope to clean this up after it is fixed.
        if (Build.VERSION.SDK_INT >= 36) {
            info.setExpandedState(EXPANDED_STATE_COLLAPSED);
        }
    }

    @Override
    public boolean performAccessibilityAction(int action, @Nullable Bundle arguments) {
        boolean superResult = super.performAccessibilityAction(action, arguments);
        if (superResult) return true; // If action was already handled, return true.
        if (action == AccessibilityAction.ACTION_EXPAND.getId()) {
            performClick(); // EXPAND action is synonymous with click.
            return true;
        }
        return super.performAccessibilityAction(action, arguments);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (isGoForward(event)) {
            performClick(); // Navigate into the submenu.
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }
}
