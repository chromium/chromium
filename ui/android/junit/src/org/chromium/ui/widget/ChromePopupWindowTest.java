// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.util.ReflectionHelpers;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.ui.base.UiAndroidFeatures;

/** Unit tests for {@link ChromePopupWindow}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class ChromePopupWindowTest {
    private Activity mActivity;
    private View mAnchorView;

    private static class TestActivity extends Activity {
        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            return keyCode == KeyEvent.KEYCODE_T && event.isCtrlPressed();
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mAnchorView = new View(mActivity);
        mActivity.setContentView(mAnchorView);
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    @Test
    @EnableFeatures(UiAndroidFeatures.ANDROID_UNIFIED_KEYBOARD_SHORTCUTS)
    public void testForwardsUnhandledShortcutsAndDismissesHierarchy() {
        ChromePopupWindow rootPopup = new ChromePopupWindow(mActivity);
        rootPopup.setContentView(new FrameLayout(mActivity));
        rootPopup.showAtLocation(mAnchorView, Gravity.NO_GRAVITY, 0, 0);

        ChromePopupWindow flyoutPopup = new ChromePopupWindow(mActivity);
        FrameLayout flyoutContent = new FrameLayout(mActivity);
        flyoutPopup.setContentView(flyoutContent);
        flyoutPopup.showAtLocation(mAnchorView, Gravity.NO_GRAVITY, 0, 0);
        View decorView = flyoutContent.getRootView();

        // Unhandled non-shortcut keys, key-up events, and key repeats leave popups open.
        assertFalse(
                dispatchUnhandledKey(
                        decorView, new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A)));
        assertFalse(
                dispatchUnhandledKey(
                        decorView,
                        new KeyEvent(
                                0,
                                0,
                                KeyEvent.ACTION_UP,
                                KeyEvent.KEYCODE_T,
                                0,
                                KeyEvent.META_CTRL_ON)));
        assertFalse(
                dispatchUnhandledKey(
                        decorView,
                        new KeyEvent(
                                0,
                                0,
                                KeyEvent.ACTION_DOWN,
                                KeyEvent.KEYCODE_T,
                                1,
                                KeyEvent.META_CTRL_ON)));
        assertTrue(rootPopup.isShowing());
        assertTrue(flyoutPopup.isShowing());

        // Handled browser shortcut dismisses the open popup hierarchy.
        KeyEvent ctrlT =
                new KeyEvent(
                        0, 0, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_T, 0, KeyEvent.META_CTRL_ON);
        assertTrue(dispatchUnhandledKey(decorView, ctrlT));
        assertFalse(rootPopup.isShowing());
        assertFalse(flyoutPopup.isShowing());
    }

    private static boolean dispatchUnhandledKey(View view, KeyEvent event) {
        return ReflectionHelpers.callInstanceMethod(
                        view,
                        "dispatchUnhandledKeyEvent",
                        ClassParameter.from(KeyEvent.class, event))
                != null;
    }
}
