// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM_WITH_SUBMENU;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.IS_HIGHLIGHTED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;
import android.view.View.OnClickListener;
import android.widget.ListView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.listmenu.ListMenuFlyoutController.FlyoutHandler;
import org.chromium.ui.listmenu.ListMenuFlyoutController.FlyoutPopupEntry;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link ListMenuFlyoutController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ListMenuFlyoutControllerUnitTest {

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

    // This is the class under test
    private ListMenuFlyoutController mFlyoutController;

    private ListItem mListItemWithModelClickCallback;
    private ListItem mSubmenuLevel1;
    private ListItem mSubmenu0Child1;
    private ListItem mSubmenuLevel0;
    private ListItem mListItemWithoutModelClickCallback;

    @Before
    public void setUp() {
        mFlyoutController = new ListMenuFlyoutController(mFlyoutHandler);

        mListItemWithModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(ENABLED, true)
                                .with(TITLE, SUBMENU_1_CHILD_0)
                                .with(CLICK_LISTENER, mItemClickListener)
                                .with(IS_HIGHLIGHTED, false)
                                .build());

        mSubmenuLevel1 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, SUBMENU_LEVEL_1)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(mListItemWithModelClickCallback))
                                .with(IS_HIGHLIGHTED, false)
                                .build());

        mSubmenu0Child1 =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE, SUBMENU_0_CHILD_1)
                                .with(ENABLED, true)
                                .with(MENU_ITEM_ID, TEST_MENU_ITEM_ID)
                                .with(IS_HIGHLIGHTED, false)
                                .build());
        mSubmenuLevel0 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, SUBMENU_LEVEL_0)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(mSubmenuLevel1, mSubmenu0Child1))
                                .with(IS_HIGHLIGHTED, false)
                                .build());

        mListItemWithoutModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE, TOP_LEVEL_ITEM)
                                .with(ENABLED, true)
                                .with(MENU_ITEM_ID, TEST_MENU_ITEM_ID)
                                .with(IS_HIGHLIGHTED, false)
                                .build());

        when(mListView.getContext()).thenReturn(ApplicationProvider.getApplicationContext());
        when(mListView.getHandler()).thenReturn(new Handler(Looper.getMainLooper()));
    }

    @Test
    public void hoverShowsFlyoutAfterDelay() {
        // Create the main menu popup window (level 0).
        List<FlyoutPopupEntry<Object>> dialogs = new ArrayList<>();
        dialogs.add(new FlyoutPopupEntry(null, new Object()));
        when(mFlyoutHandler.getFlyoutWindows()).thenReturn(dialogs);

        // Start hover on one of the items on the main menu (level 0).
        triggerHoverEnter(mSubmenuLevel0, 0, List.of(mSubmenuLevel0));

        // Verify that before the delay, no new window is added.
        verify(mFlyoutHandler, never()).addFlyoutWindow(any(), any());

        // Wait for the UI delay.
        waitForUiDelay();

        // Verify that the call to create a new popup (level 1) is called.
        verify(mFlyoutHandler).addFlyoutWindow(mSubmenuLevel0, mListView);
        dialogs.add(new FlyoutPopupEntry(mSubmenuLevel0, new Object()));

        // Hover on an item inside the level 1 popup for long enough.
        triggerHoverEnter(mSubmenuLevel1, 1, List.of(mSubmenuLevel0, mSubmenuLevel1));
        waitForUiDelay();

        // Verify that the call to create another popup (level 2) is called.
        verify(mFlyoutHandler).addFlyoutWindow(mSubmenuLevel1, mListView);
    }

    @Test
    public void hoverOnNewItemClosesAllDescendentPopups() {
        List<FlyoutPopupEntry<Object>> dialogs = new ArrayList<>();
        when(mFlyoutHandler.getFlyoutWindows()).thenReturn(dialogs);

        // Create level 0, 1, and 2 popup windows.
        dialogs.add(new FlyoutPopupEntry(null, new Object())); // Level 0 popup.
        dialogs.add(new FlyoutPopupEntry(mSubmenuLevel0, new Object())); // Level 1 popup.
        dialogs.add(new FlyoutPopupEntry(mSubmenuLevel1, new Object())); // Level 2 popup.

        // Hover on a different item on the level 0 popup.
        triggerHoverEnter(
                mListItemWithoutModelClickCallback, 0, List.of(mListItemWithoutModelClickCallback));
        waitForUiDelay();

        // Popups of level 1 and 2 should be removed.
        verify(mFlyoutHandler).removeFlyoutWindows(1);

        // Create level 0, 1, and 2 popup windows again.
        dialogs = new ArrayList<>();
        dialogs.add(new FlyoutPopupEntry(null, new Object())); // Level 0 popup.
        dialogs.add(new FlyoutPopupEntry(mSubmenuLevel0, new Object())); // Level 1 popup.
        dialogs.add(new FlyoutPopupEntry(mSubmenuLevel1, new Object())); // Level 2 popup.

        // Hover on a different item on the level 1 popup.
        triggerHoverEnter(mSubmenu0Child1, 1, List.of(mSubmenuLevel0, mSubmenu0Child1));
        waitForUiDelay();

        // Level 2 popup should be removed, but level 1 popup should remain.
        verify(mFlyoutHandler).removeFlyoutWindows(2);
    }

    @Test
    public void hoverOnOriginalItemKeepsDirectChild() {
        List<FlyoutPopupEntry<Object>> dialogs = new ArrayList<>();
        when(mFlyoutHandler.getFlyoutWindows()).thenReturn(dialogs);

        // Create level 0, 1, and 2 popup windows.
        dialogs.add(new FlyoutPopupEntry(null, new Object())); // Level 0 popup.
        dialogs.add(new FlyoutPopupEntry(mSubmenuLevel0, new Object())); // Level 1 popup.
        dialogs.add(new FlyoutPopupEntry(mSubmenuLevel1, new Object())); // Level 2 popup.

        // Hover on the original item on the level 0 popup.
        triggerHoverEnter(mSubmenuLevel0, 0, List.of(mSubmenuLevel0));
        waitForUiDelay();

        // Level 2 popup should be removed, but level 1 popup should remain.
        verify(mFlyoutHandler).removeFlyoutWindows(2);
        dialogs.subList(2, dialogs.size()).clear();
    }

    @Test
    public void testHighlightPath_UpdatesOnHoverSequentially() {
        // Case 1: Hover root item 1 ("Submenu level 0")
        triggerHoverEnter(mSubmenuLevel0, 0, List.of(mSubmenuLevel0));
        assertEquals(
                "mSubmenuLevel0 should be highlighted",
                true,
                mSubmenuLevel0.model.get(IS_HIGHLIGHTED));
        assertEquals(
                "Sibling should not be highlighted",
                false,
                mListItemWithoutModelClickCallback.model.get(IS_HIGHLIGHTED));

        // Case 2: Hover a child ("Submenu level 1")
        triggerHoverEnter(mSubmenuLevel1, 1, List.of(mSubmenuLevel0, mSubmenuLevel1));
        assertEquals(
                "Parent mSubmenuLevel0 should stay highlighted",
                true,
                mSubmenuLevel0.model.get(IS_HIGHLIGHTED));
        assertEquals(
                "Child mSubmenuLevel1 should be highlighted",
                true,
                mSubmenuLevel1.model.get(IS_HIGHLIGHTED));
        assertEquals(
                "Sibling of parent should not be highlighted",
                false,
                mListItemWithoutModelClickCallback.model.get(IS_HIGHLIGHTED));

        // Case 3: Hover a "niece" ("Submenu 0 child 1")
        triggerHoverEnter(mSubmenu0Child1, 1, List.of(mSubmenuLevel0, mSubmenu0Child1));
        assertEquals(
                "Common ancestor mSubmenuLevel0 should stay highlighted",
                true,
                mSubmenuLevel0.model.get(IS_HIGHLIGHTED));
        assertEquals(
                "Old branch mSubmenuLevel1 should be de-highlighted",
                false,
                mSubmenuLevel1.model.get(IS_HIGHLIGHTED));
        assertEquals(
                "New branch mSubmenu0Child1 should be highlighted",
                true,
                mSubmenu0Child1.model.get(IS_HIGHLIGHTED));

        // Case 4: Hover a grandchild ("Submenu 1 child 0")
        triggerHoverEnter(
                mListItemWithModelClickCallback,
                2,
                List.of(mSubmenuLevel0, mSubmenuLevel1, mListItemWithModelClickCallback));
        assertEquals(
                "Ancestor mSubmenuLevel0 should stay highlighted",
                true,
                mSubmenuLevel0.model.get(IS_HIGHLIGHTED));
        assertEquals(
                "Old branch mSubmenu0Child1 should be de-highlighted",
                false,
                mSubmenu0Child1.model.get(IS_HIGHLIGHTED));
        assertEquals(
                "Parent mSubmenuLevel1 should be highlighted",
                true,
                mSubmenuLevel1.model.get(IS_HIGHLIGHTED));
        assertEquals(
                "Grandchild item should be highlighted",
                true,
                mListItemWithModelClickCallback.model.get(IS_HIGHLIGHTED));

        // Case 5: Hover an ancestor ("Submenu level 1")
        triggerHoverEnter(mSubmenuLevel1, 1, List.of(mSubmenuLevel0, mSubmenuLevel1));
        assertEquals(
                "Ancestor mSubmenuLevel0 should stay highlighted",
                true,
                mSubmenuLevel0.model.get(IS_HIGHLIGHTED));
        assertEquals(
                "Item mSubmenuLevel1 should stay highlighted",
                true,
                mSubmenuLevel1.model.get(IS_HIGHLIGHTED));
        assertEquals(
                "Old child mListItemWithModelClickCallback should be de-highlighted",
                false,
                mListItemWithModelClickCallback.model.get(IS_HIGHLIGHTED));

        // Case 6: Hover a sibling of the root ("Top level item")
        triggerHoverEnter(
                mListItemWithoutModelClickCallback, 0, List.of(mListItemWithoutModelClickCallback));
        assertEquals(
                "Old root mSubmenuLevel0 should be de-highlighted",
                false,
                mSubmenuLevel0.model.get(IS_HIGHLIGHTED));
        assertEquals(
                "Old child mSubmenuLevel1 should be de-highlighted",
                false,
                mSubmenuLevel1.model.get(IS_HIGHLIGHTED));
        assertEquals(
                "New root mListItemWithoutModelClickCallback should be highlighted",
                true,
                mListItemWithoutModelClickCallback.model.get(IS_HIGHLIGHTED));
    }

    private void triggerHoverEnter(ListItem item, int level, List<ListItem> path) {
        mFlyoutController.handleHoverEvent(
                createHoverEnterEvent(), item, mListView, level, false, path);
    }

    private MotionEvent createHoverEnterEvent() {
        return MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 1.f, 1.f, 0);
    }

    private static void waitForUiDelay() {
        shadowOf(Looper.getMainLooper()).idle();
        ShadowLooper.runMainLooperOneTask();
    }
}
