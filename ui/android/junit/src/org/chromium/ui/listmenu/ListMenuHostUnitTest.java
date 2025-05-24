// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Spy;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
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

    private void showMenuForButton(ListMenuButton button) {
        button.setAttachedToWindowForTesting();
        button.showMenu();
        ShadowLooper.idleMainLooper();
        assertNotNull(mSpyPopupMenu);
    }

    private void dismissMenu(ListMenuButton button) {
        button.dismiss();
        ShadowLooper.idleMainLooper();
        assertNull(mSpyPopupMenu);
    }
}
