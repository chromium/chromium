// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.content.Context;
import android.os.Build;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.ui.R;
import org.chromium.ui.base.MotionEventTestUtils;

/** Unit tests for {@link ListMenuButton}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ListMenuButtonUnitTest {

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void testA11yLabel() {
        ListMenuButton button = new ListMenuButton(mContext, null);

        button.setContentDescriptionContext("");
        Assert.assertEquals(
                mContext.getString(R.string.accessibility_toolbar_btn_menu),
                button.getContentDescription());

        String title = "Test title";
        button.setContentDescriptionContext(title);
        Assert.assertEquals(
                mContext.getString(R.string.accessibility_list_menu_button, title),
                button.getContentDescription());
    }

    @Test
    public void testTriggerShowMenuTwice() {
        ListMenuButton button = createListMenuButton();

        // Expect no crash when calling showMenu twice.
        button.showMenu();
        button.showMenu();
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    public void testSecondaryClick() {
        CallbackHelper longClickHelper = new CallbackHelper();
        ListMenuButton button = new ListMenuButton(mContext, null);
        button.setOnLongClickListener(
                (v) -> {
                    longClickHelper.notifyCalled();
                    return true;
                });
        MotionEvent secondaryClickEvent = MotionEventTestUtils.getTrackRightClickEvent();
        button.onGenericMotionEvent(secondaryClickEvent);
        Assert.assertEquals(1, longClickHelper.getCallCount());
    }

    @Test
    public void testMenuOpenSetsPressedState() {
        ListMenuButton button = createListMenuButton();

        Assert.assertFalse("Button should not be pressed initially.", button.isPressed());
        button.showMenu();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue("Button should be pressed when menu is open.", button.isPressed());

        button.dismiss();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse(
                "Button should not be pressed after menu is dismissed.", button.isPressed());
    }

    @Test
    public void testMenuOpenDoesNotSetPressedStateWhenDisabled() {
        ListMenuButton button = createListMenuButton();
        button.setMaintainPressedStateWhenMenuOpen(false);

        Assert.assertFalse("Button should not be pressed initially.", button.isPressed());
        button.showMenu();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertFalse(
                "Button should not be pressed when menu is open and maintain pressed state is"
                        + " disabled.",
                button.isPressed());
    }

    @Test
    public void testMenuOpenMaintainsFocusStateByDefault() {
        ListMenuButton button = createListMenuButton();

        button.setFocusable(true);
        button.setFocusableInTouchMode(true);
        button.requestFocus();
        Assert.assertTrue("Button should be focused.", button.isFocused());
        button.showMenu();

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue("Button should remain focused when menu is open.", button.isFocused());
    }

    private ListMenuButton createListMenuButton() {
        ViewGroup contentView = new FrameLayout(mContext);
        ListMenuButton button = new ListMenuButton(mContext, null);
        button.setAttachedToWindowForTesting();
        button.setDelegate(
                () ->
                        new ListMenu() {
                            @Override
                            public View getContentView() {
                                return contentView;
                            }

                            @Override
                            public void addContentViewClickRunnable(Runnable runnable) {}

                            @Override
                            public int getMaxItemWidth() {
                                return 0;
                            }
                        },
                true);
        return button;
    }
}
