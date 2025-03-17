// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.RootMatchers.isPlatformPopup;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.view.View;

import org.hamcrest.Matchers;
import org.mockito.Mockito;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.ui.widget.AnchoredPopupWindow;

/** Test utilities for ListMenu. */
public final class ListMenuTestUtils {

    /**
     * Set the callback to capture the current popup window when its shown / dismissed and return
     * the Mockito spy. This is useful for features that wants to verify the menu properties within
     * the menu.
     *
     * Example usage:
     * <pre>
     * {@code
     * public void myTest() {
     *     captureListMenuHostSpy(menu -> mSpyPopupMenu = menu);
     *     // Call some method that shows the menu
     *     assertNotNull(mSpyPopupMenu);
     *
     *     verify(mSpyListMenu).setMaxWidth(anyInt());
     * }
     */
    public static void captorPopupWindowSpy(Callback<AnchoredPopupWindow> popupWindowCallback) {
        ListMenuHost.setMenuChangedListenerForTesting(
                menu -> {
                    if (menu != null) menu = Mockito.spy(menu);
                    popupWindowCallback.onResult(menu);
                    return menu;
                });
    }

    /** Long click the |menuAnchor| view to show the List Menu. */
    public static void longClickAndWaitForListMenu(View menuAnchor) {
        waitUntilMenuShowup(
                () -> ThreadUtils.runOnUiThreadBlocking(() -> menuAnchor.performLongClick()));
    }

    /**
     * Trigger the ListMenu via |menuLauncher| and wait until the menu is shown.
     *
     * @param menuLauncher A custom callback to launch the list menu.
     */
    public static void waitUntilMenuShowup(Runnable menuLauncher) {
        PayloadCallbackHelper<AnchoredPopupWindow> popupMenu = new PayloadCallbackHelper<>();
        ListMenuHost.setMenuChangedListenerForTesting(
                menu -> {
                    popupMenu.notifyCalled(menu);
                    return menu;
                });

        menuLauncher.run();
        AnchoredPopupWindow menu = popupMenu.getOnlyPayloadBlocking();
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                "Menu never shows up.",
                                menu != null && menu.isShowing(),
                                Matchers.is(true)));
    }

    /**
     * Click the list menu item assuming the menu is shown.
     *
     * @param text The text option of the desired list menu item.
     */
    public static void invokeMenuItem(String text) {
        // Invoke menu item since menu is visible
        onView(withText(text)).inRoot(isPlatformPopup()).perform(click());
    }
}
