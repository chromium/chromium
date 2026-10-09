// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Spy;
import org.robolectric.Robolectric;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.ui.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.widget.AnchoredPopupWindow;

/** Unit test for {@link ListMenuHost}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ListMenuHostUnitTest {

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;
    private @Spy AnchoredPopupWindow mSpyPopupMenu;
    private ListMenuDelegate mMenuDelegate;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mActivity.setContentView(R.layout.list_menu_button_unittest);
        ListMenuTestUtils.captorPopupWindowSpy(host -> mSpyPopupMenu = host);

        // The menu will not be tested in the test body.
        mMenuDelegate =
                new ListMenuDelegate() {
                    @Override
                    public ListMenu getListMenu() {
                        return new ListMenu() {
                            @Override
                            public View getContentView() {
                                return new FrameLayout(mActivity);
                            }

                            @Override
                            public void addContentViewClickRunnable(Runnable runnable) {}

                            @Override
                            public int getMaxItemWidth() {
                                return 0;
                            }
                        };
                    }
                };
    }

    @Test
    public void checkAnchorAttributes() {
        ListMenuButton btnDefault = mActivity.findViewById(R.id.button_default);
        btnDefault.setDelegate(mMenuDelegate);

        showMenuForButton(btnDefault);
        verify(mSpyPopupMenu, times(0)).setAnimationStyle(anyInt());
        dismissMenu(btnDefault);
    }

    @Test
    public void checkAnchorAttributes_End() {
        ListMenuButton btnMenuEnd = mActivity.findViewById(R.id.button_menu_end);
        btnMenuEnd.setDelegate(mMenuDelegate);

        showMenuForButton(btnMenuEnd);
        verify(mSpyPopupMenu, atLeastOnce()).setAnimationStyle(R.style.EndIconMenuAnim);
        dismissMenu(btnMenuEnd);
    }

    @Test
    public void checkAnchorAttributes_Start() {
        ListMenuButton btnMenuStart = mActivity.findViewById(R.id.button_menu_start);
        btnMenuStart.setDelegate(mMenuDelegate);

        showMenuForButton(btnMenuStart);
        verify(mSpyPopupMenu, atLeastOnce()).setAnimationStyle(R.style.StartIconMenuAnim);
        dismissMenu(btnMenuStart);
    }

    @Test
    public void testShowMenuWithViewBuilder() {
        ListMenuButton btnDefault = mActivity.findViewById(R.id.button_default);
        btnDefault.setDelegate(mMenuDelegate);

        showMenuForButton(btnDefault);
        assertNotNull(
                "Popup menu should have content view from ViewBuilder",
                mSpyPopupMenu.getContentView());
        dismissMenu(btnDefault);
    }

    @Test
    public void testShowMenuWithIncompatibleContext_asserts() {
        Activity otherActivity = Robolectric.buildActivity(Activity.class).setup().get();
        ListMenuButton btnDefault = mActivity.findViewById(R.id.button_default);
        btnDefault.setDelegate(
                new ListMenuDelegate() {
                    @Override
                    public ListMenu getListMenu() {
                        return new ListMenu() {
                            @Override
                            public View getContentView() {
                                return new FrameLayout(otherActivity);
                            }

                            @Override
                            public void addContentViewClickRunnable(Runnable runnable) {}

                            @Override
                            public int getMaxItemWidth() {
                                return 0;
                            }
                        };
                    }
                });

        try {
            assertThrows(AssertionError.class, () -> showMenuForButton(btnDefault));
        } finally {
            if (mSpyPopupMenu != null) {
                mSpyPopupMenu.onDismissForTesting(false);
            }
            btnDefault.dismiss();
        }
    }

    @Test
    public void testShowMenuWithApplicationContextContentView_doesNotAssert() {
        ListMenuButton btnDefault = mActivity.findViewById(R.id.button_default);
        btnDefault.setDelegate(
                new ListMenuDelegate() {
                    @Override
                    public ListMenu getListMenu() {
                        return new ListMenu() {
                            @Override
                            public View getContentView() {
                                return new FrameLayout(ContextUtils.getApplicationContext());
                            }

                            @Override
                            public void addContentViewClickRunnable(Runnable runnable) {}

                            @Override
                            public int getMaxItemWidth() {
                                return 0;
                            }
                        };
                    }
                });

        showMenuForButton(btnDefault);
        assertNotNull(
                "Popup menu should have content view from ViewBuilder with application context",
                mSpyPopupMenu.getContentView());
        dismissMenu(btnDefault);
    }

    @Test
    public void testShowMenuWithNonActivityContext_doesNotAssert() {
        Context appContext = ContextUtils.getApplicationContext();
        Context themedAppContext =
                new ContextThemeWrapper(appContext, R.style.Theme_AppCompat_DayNight);
        ListMenuButton button = new ListMenuButton(themedAppContext, null);
        button.setDelegate(mMenuDelegate);

        showMenuForButton(button);
        assertNotNull(
                "Popup menu should have content view from ViewBuilder with non-Activity context",
                mSpyPopupMenu.getContentView());
        dismissMenu(button);
    }

    private void showMenuForButton(ListMenuButton button) {
        button.setAttachedToWindowForTesting();
        button.showMenu();
        RobolectricUtil.runAllBackgroundAndUi();
        assertNotNull(mSpyPopupMenu);
        assertTrue("Button should be in pressed state when menu is visible.", button.isPressed());
    }

    private void dismissMenu(ListMenuButton button) {
        button.dismiss();
        RobolectricUtil.runAllBackgroundAndUi();
        assertNull(mSpyPopupMenu);
        assertFalse(
                "Button should no longer be pressed once menu is dismissed.", button.isPressed());
    }
}
