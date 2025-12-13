// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.hierarchicalmenu;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.ui.hierarchicalmenu.HierarchicalMenuTestUtils.ALL_MENU_ITEM_KEYS;
import static org.chromium.ui.hierarchicalmenu.HierarchicalMenuTestUtils.ALL_SUBMENU_ITEM_KEYS;
import static org.chromium.ui.hierarchicalmenu.HierarchicalMenuTestUtils.CLICK_LISTENER;
import static org.chromium.ui.hierarchicalmenu.HierarchicalMenuTestUtils.ENABLED;
import static org.chromium.ui.hierarchicalmenu.HierarchicalMenuTestUtils.IS_HIGHLIGHTED;
import static org.chromium.ui.hierarchicalmenu.HierarchicalMenuTestUtils.MENU_ITEM;
import static org.chromium.ui.hierarchicalmenu.HierarchicalMenuTestUtils.MENU_ITEM_ID;
import static org.chromium.ui.hierarchicalmenu.HierarchicalMenuTestUtils.MENU_ITEM_SUBMENU_HEADER;
import static org.chromium.ui.hierarchicalmenu.HierarchicalMenuTestUtils.MENU_ITEM_WITH_SUBMENU;
import static org.chromium.ui.hierarchicalmenu.HierarchicalMenuTestUtils.SUBMENU_ITEMS;
import static org.chromium.ui.hierarchicalmenu.HierarchicalMenuTestUtils.TITLE;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.view.View.OnClickListener;
import android.widget.ListView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.hierarchicalmenu.FlyoutController.FlyoutHandler;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController.SubmenuHeaderFactory;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link FlyoutController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FlyoutControllerUnitTest {

    private static final int TEST_MENU_ITEM_ID = 3; // Arbitrary int for testing
    private static final String TOP_LEVEL_ITEM = "Top level item";
    private static final String SUBMENU_LEVEL_0 = "Submenu level 0";
    private static final String SUBMENU_0_CHILD_1 = "Submenu 0 child 1";
    private static final String SUBMENU_LEVEL_1 = "Submenu level 1";
    private static final String SUBMENU_1_CHILD_0 = "Submenu 1 child 0";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OnClickListener mItemClickListener;
    @Mock private ListView mListView;
    @Mock private FlyoutHandler<Object> mFlyoutHandler;

    private FlyoutController mFlyoutController;

    private ListItem mListItemWithModelClickCallback;
    private ListItem mSubmenuLevel1;
    private ListItem mSubmenu0Child1;
    private ListItem mSubmenuLevel0;
    private ListItem mListItemWithoutModelClickCallback;
    private HierarchicalMenuController mHierarchicalMenuController;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();

        HierarchicalMenuKeyProvider keyProvider = HierarchicalMenuTestUtils.createKeyProvider();
        SubmenuHeaderFactory headerFactory =
                (clickedItem, backRunnable) -> {
                    PropertyModel.Builder builder =
                            new PropertyModel.Builder(ALL_SUBMENU_ITEM_KEYS);
                    HierarchicalMenuController.populateDefaultHeaderProperties(
                            builder, keyProvider, clickedItem.model.get(TITLE), backRunnable);
                    return new ListItem(MENU_ITEM_SUBMENU_HEADER, builder.build());
                };

        mHierarchicalMenuController =
                new HierarchicalMenuController(mContext, keyProvider, headerFactory);

        mFlyoutController =
                new FlyoutController(
                        mFlyoutHandler,
                        HierarchicalMenuTestUtils.createKeyProvider(),
                        new Object(),
                        mHierarchicalMenuController);

        mListItemWithModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ALL_MENU_ITEM_KEYS)
                                .with(ENABLED, true)
                                .with(TITLE, SUBMENU_1_CHILD_0)
                                .with(CLICK_LISTENER, mItemClickListener)
                                .with(IS_HIGHLIGHTED, false)
                                .build());

        mSubmenuLevel1 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ALL_SUBMENU_ITEM_KEYS)
                                .with(TITLE, SUBMENU_LEVEL_1)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(mListItemWithModelClickCallback))
                                .with(IS_HIGHLIGHTED, false)
                                .build());

        mSubmenu0Child1 =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ALL_MENU_ITEM_KEYS)
                                .with(TITLE, SUBMENU_0_CHILD_1)
                                .with(ENABLED, true)
                                .with(MENU_ITEM_ID, TEST_MENU_ITEM_ID)
                                .with(IS_HIGHLIGHTED, false)
                                .build());
        mSubmenuLevel0 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ALL_SUBMENU_ITEM_KEYS)
                                .with(TITLE, SUBMENU_LEVEL_0)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(mSubmenuLevel1, mSubmenu0Child1))
                                .with(IS_HIGHLIGHTED, false)
                                .build());

        mListItemWithoutModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ALL_MENU_ITEM_KEYS)
                                .with(TITLE, TOP_LEVEL_ITEM)
                                .with(ENABLED, true)
                                .with(MENU_ITEM_ID, TEST_MENU_ITEM_ID)
                                .with(IS_HIGHLIGHTED, false)
                                .build());

        when(mListView.getContext()).thenReturn(mContext);
        when(mListView.getHandler()).thenReturn(new Handler(Looper.getMainLooper()));
    }

    @Test
    public void hoverShowsFlyoutAfterDelay() {
        // Start hover on one of the items on the main menu (level 0).
        triggerHoverEnter(mSubmenuLevel0, 0, List.of(mSubmenuLevel0));

        // Verify that before the delay, no new window is added.
        Assert.assertEquals("There should be 1 popup.", 1, mFlyoutController.getNumberOfPopups());
        verify(mFlyoutHandler, never()).createAndShowFlyoutPopup(any(), any(), any());

        // Wait for the UI delay.
        waitForUiDelay();

        // Verify that the call to create a new popup (level 1) is called.
        verify(mFlyoutHandler).createAndShowFlyoutPopup(eq(mSubmenuLevel0), eq(mListView), any());
        Assert.assertEquals("There should be 2 popups.", 2, mFlyoutController.getNumberOfPopups());

        // Hover on an item inside the level 1 popup for long enough.
        triggerHoverEnter(mSubmenuLevel1, 1, List.of(mSubmenuLevel0, mSubmenuLevel1));
        waitForUiDelay();

        // Verify that the call to create another popup (level 2) is called.
        verify(mFlyoutHandler).createAndShowFlyoutPopup(eq(mSubmenuLevel1), eq(mListView), any());
        Assert.assertEquals("There should be 3 popups.", 3, mFlyoutController.getNumberOfPopups());
    }

    @Test
    public void hoverOnNewItemClosesAllDescendentPopups() {
        // Create level 1 and 2 popup windows.
        triggerHoverEnter(mSubmenuLevel0, 0, List.of(mSubmenuLevel0));
        waitForUiDelay();
        triggerHoverEnter(mSubmenuLevel1, 1, List.of(mSubmenuLevel0, mSubmenuLevel1));
        waitForUiDelay();

        // Hover on a different item on the level 0 popup.
        triggerHoverEnter(
                mListItemWithoutModelClickCallback, 0, List.of(mListItemWithoutModelClickCallback));
        waitForUiDelay();

        // Popups of level 1 and 2 should be removed.
        Assert.assertEquals("There should be 1 popup.", 1, mFlyoutController.getNumberOfPopups());

        // Create level 1 and 2 popup windows.
        triggerHoverEnter(mSubmenuLevel0, 0, List.of(mSubmenuLevel0));
        waitForUiDelay();
        triggerHoverEnter(mSubmenuLevel1, 1, List.of(mSubmenuLevel0, mSubmenuLevel1));
        waitForUiDelay();

        // Hover on a different item on the level 1 popup.
        triggerHoverEnter(mSubmenu0Child1, 1, List.of(mSubmenuLevel0, mSubmenu0Child1));
        waitForUiDelay();

        // Level 2 popup should be removed, but level 1 popup should remain.
        Assert.assertEquals("There should be 2 popups.", 2, mFlyoutController.getNumberOfPopups());
    }

    @Test
    public void hoverOnOriginalItemKeepsDirectChild() {
        // Create level 1 and 2 popup windows.
        triggerHoverEnter(mSubmenuLevel0, 0, List.of(mSubmenuLevel0));
        waitForUiDelay();
        triggerHoverEnter(mSubmenuLevel1, 1, List.of(mSubmenuLevel0, mSubmenuLevel1));
        waitForUiDelay();

        // Hover on the original item on the level 0 popup.
        triggerHoverEnter(mSubmenuLevel0, 0, List.of(mSubmenuLevel0));
        waitForUiDelay();

        // Level 2 popup should be removed, but level 1 popup should remain.
        Assert.assertEquals("There should be 2 popups.", 2, mFlyoutController.getNumberOfPopups());
    }

    private void triggerHoverEnter(ListItem item, int level, List<ListItem> path) {
        mFlyoutController.onItemHovered(item, mListView, level, path);
    }

    private static void waitForUiDelay() {
        shadowOf(Looper.getMainLooper()).idle();
        ShadowLooper.runMainLooperOneTask();
    }
}
