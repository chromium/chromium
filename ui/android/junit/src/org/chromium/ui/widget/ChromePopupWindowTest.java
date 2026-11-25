// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.widget.FrameLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
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

    @Test
    @DisableFeatures(UiAndroidFeatures.ANDROID_UNIFIED_KEYBOARD_SHORTCUTS)
    public void testDismissAllPopupsForTesting() {
        ChromePopupWindow locationPopup = new ChromePopupWindow(mActivity);
        locationPopup.setContentView(new FrameLayout(mActivity));
        locationPopup.showAtLocation(mAnchorView, Gravity.NO_GRAVITY, 0, 0);

        ChromePopupWindow dropdownPopup = new ChromePopupWindow(mActivity);
        dropdownPopup.setContentView(new FrameLayout(mActivity));
        dropdownPopup.showAsDropDown(mAnchorView);

        ChromePopupWindow alreadyDismissedPopup = new ChromePopupWindow(mActivity);
        alreadyDismissedPopup.setContentView(new FrameLayout(mActivity));
        alreadyDismissedPopup.showAtLocation(mAnchorView, Gravity.NO_GRAVITY, 0, 0);
        alreadyDismissedPopup.dismiss();

        assertTrue(locationPopup.isShowing());
        assertTrue(dropdownPopup.isShowing());
        assertFalse(alreadyDismissedPopup.isShowing());

        assertEquals(2, ChromePopupWindow.dismissAllPopupsForTesting());
        assertEquals(0, ChromePopupWindow.dismissAllPopupsForTesting());

        assertFalse(locationPopup.isShowing());
        assertFalse(dropdownPopup.isShowing());
        assertFalse(alreadyDismissedPopup.isShowing());
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
    public void testAllowOverlapCaptionBar_withBackgroundDrawable_clearsOnlyCaptionBarInset() {
        ChromePopupWindow popup = createPopup(new ColorDrawable(Color.WHITE));
        popup.setAllowOverlapCaptionBar(true);
        popup.showAtLocation(mAnchorView, Gravity.TOP | Gravity.START, /* x= */ 0, /* y= */ 0);

        View contentView = popup.getContentView();
        // Verify PopupWindow wrapped contentView in PopupBackgroundView.
        assertNotEquals(contentView.getParent(), contentView.getRootView());
        WindowManager.LayoutParams lp =
                (WindowManager.LayoutParams) contentView.getRootView().getLayoutParams();
        assertEquals(
                WindowInsets.Type.systemBars() & ~WindowInsets.Type.captionBar(),
                lp.getFitInsetsTypes());
        popup.dismiss();
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
    public void testAllowOverlapCaptionBar_withoutBackgroundDrawable_clearsOnlyCaptionBarInset() {
        ChromePopupWindow popup = createPopup(/* background= */ null);
        popup.setAllowOverlapCaptionBar(true);
        popup.showAtLocation(mAnchorView, Gravity.TOP | Gravity.START, /* x= */ 0, /* y= */ 0);

        WindowManager.LayoutParams lp =
                (WindowManager.LayoutParams) popup.getContentView().getRootView().getLayoutParams();
        assertEquals(
                WindowInsets.Type.systemBars() & ~WindowInsets.Type.captionBar(),
                lp.getFitInsetsTypes());
        popup.dismiss();
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
    public void testAllowOverlapCaptionBar_showAsDropDown_clearsOnlyCaptionBarInset() {
        ChromePopupWindow popup = createPopup(new ColorDrawable(Color.WHITE));
        popup.setAllowOverlapCaptionBar(/* allow= */ true);
        popup.showAsDropDown(mAnchorView);

        WindowManager.LayoutParams lp =
                (WindowManager.LayoutParams) popup.getContentView().getRootView().getLayoutParams();
        assertEquals(
                WindowInsets.Type.systemBars() & ~WindowInsets.Type.captionBar(),
                lp.getFitInsetsTypes());
        popup.dismiss();
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
    public void testAllowOverlapCaptionBar_false_preservesSystemBarsInsets() {
        ChromePopupWindow popup = createPopup(new ColorDrawable(Color.WHITE));
        popup.setAllowOverlapCaptionBar(false);
        popup.showAtLocation(mAnchorView, Gravity.TOP | Gravity.START, /* x= */ 0, /* y= */ 0);

        WindowManager.LayoutParams lp =
                (WindowManager.LayoutParams) popup.getContentView().getRootView().getLayoutParams();
        assertEquals(WindowInsets.Type.systemBars(), lp.getFitInsetsTypes());
        popup.dismiss();
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MIN_SDK)
    public void testAllowOverlapCaptionBar_preR_showsWithoutCrash() {
        ChromePopupWindow popup = createPopup(new ColorDrawable(Color.WHITE));
        popup.setAllowOverlapCaptionBar(true);
        popup.showAtLocation(mAnchorView, Gravity.TOP | Gravity.START, /* x= */ 0, /* y= */ 0);

        assertTrue(popup.isShowing());
        popup.dismiss();
    }

    private ChromePopupWindow createPopup(@Nullable Drawable background) {
        ChromePopupWindow popup = new ChromePopupWindow(mActivity);
        popup.setContentView(new View(mActivity));
        popup.setWidth(100);
        popup.setHeight(100);
        popup.setBackgroundDrawable(background);
        return popup;
    }

    private static boolean dispatchUnhandledKey(View view, KeyEvent event) {
        return ReflectionHelpers.callInstanceMethod(
                        view,
                        "dispatchUnhandledKeyEvent",
                        ClassParameter.from(KeyEvent.class, event))
                != null;
    }
}
