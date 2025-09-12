// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM_WITH_SUBMENU;
import static org.chromium.ui.listmenu.ListItemType.SUBMENU_HEADER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.HOVER_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.IS_HIGHLIGHTED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;
import static org.chromium.ui.listmenu.ListMenuUtils.setupCallbacksRecursively;

import android.os.Handler;
import android.os.Looper;
import android.util.Pair;
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
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.listmenu.ListMenuUtils.FlyoutHandler;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for the context menu mediator. */
@RunWith(BaseRobolectricTestRunner.class)
public class ListMenuUtilsUnitTest {

    private static final int TEST_MENU_ITEM_ID = 3; // Arbitrary int for testing
    private static final String TOP_LEVEL_ITEM = "Top level item";
    private static final String SUBMENU_LEVEL_0 = "Submenu level 0";
    private static final String SUBMENU_0_CHILD_1 = "Submenu 0 child 1";
    private static final String SUBMENU_LEVEL_1 = "Submenu level 1";
    private static final String SUBMENU_1_CHILD_0 = "Submenu 1 child 0";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OnClickListener mItemClickListener;
    @Mock private Runnable mDismissDialog;
    @Mock private ListView mListView;
    @Mock private ListObservable.ListObserver<Void> mListObserver;
    @Mock private FlyoutHandler<Object> mFlyoutHandler;

    private final ModelList mModelList = new ModelList();
    private ListItem mListItemWithModelClickCallback;
    private ListItem mSubmenuLevel1;
    private ListItem mSubmenu0Child1;
    private ListItem mSubmenuLevel0;
    private ListItem mListItemWithoutModelClickCallback;

    @Before
    public void setUp() {
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
        mModelList.add(mSubmenuLevel0);

        // Add an item with no click callback
        mListItemWithoutModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE, TOP_LEVEL_ITEM)
                                .with(ENABLED, true)
                                .with(MENU_ITEM_ID, TEST_MENU_ITEM_ID)
                                .with(IS_HIGHLIGHTED, false)
                                .build());
        mModelList.add(mListItemWithoutModelClickCallback);

        when(mListView.getContext()).thenReturn(ApplicationProvider.getApplicationContext());
        when(mListView.getHandler()).thenReturn(new Handler(Looper.getMainLooper()));
    }

    @Test
    public void getItemList_submenuNavigation_noStaticHeader() {
        setupCallbacksRecursively(
                /* headerModelList= */ null,
                mModelList,
                mDismissDialog,
                /* flyoutHandler= */ null,
                /* drillDownOverrideValue= */ true);
        // Click into submenu 0
        activateClickListener(mSubmenuLevel0);
        assertEquals(
                "Expected submenu level 0 to have 3 items (1 header and 2 children)",
                3,
                mModelList.size());
        ListItem header = mModelList.get(0);
        assertEquals(
                "Expected 1st element after clicking into submenu level 0 to have header type",
                SUBMENU_HEADER,
                header.type);
        // Go back to the root level
        activateClickListener(header);
        verify(mDismissDialog, never()).run(); // Clicking into submenu and back should not dismiss
        // Verify correctness of model contents
        assertEquals("Expected root level to have 2 items", 2, mModelList.size());
        assertEquals(
                "Expected 1st element of root level to be submenu level 0",
                mSubmenuLevel0,
                mModelList.get(0));
        assertEquals(
                "Expected 2nd element of root level to be a menu item",
                mListItemWithoutModelClickCallback,
                mModelList.get(1));
        // Go into submenu 0 again
        activateClickListener(mSubmenuLevel0);
        assertEquals(
                "Expected submenu 0 to still have 3 items", // No extra header or items
                3,
                mModelList.size());
        // Go into submenu 1
        activateClickListener(mSubmenuLevel1);
        assertEquals(
                "Expected submenu 1 to have 2 items (1 header and 1 child)", // No extra header
                2,
                mModelList.size());
        // Assert correctness of contents
        assertEquals(
                "Expected 1st element after clicking into submenu level 1 to have header type",
                SUBMENU_HEADER,
                mModelList.get(0).type);
        assertEquals(
                "Expected 2nd element to be correct child",
                mListItemWithModelClickCallback,
                mModelList.get(1));
    }

    @Test
    public void getItemList_submenuNavigation_withStaticHeader() {
        // Set up the header model list
        ModelList headerModelList = new ModelList();

        // Begin test
        setupCallbacksRecursively(
                headerModelList,
                mModelList,
                mDismissDialog,
                /* flyoutHandler= */ null,
                /* drillDownOverrideValue= */ true);
        // Click into submenu 0
        activateClickListener(mSubmenuLevel0);
        assertEquals(
                "Expected header section to have 1 header for submenu level 0",
                1,
                headerModelList.size());
        assertEquals(
                "Expected content section to have 2 children for submenu level 0",
                2,
                mModelList.size());
        ListItem header = headerModelList.get(0);
        assertEquals(
                "Expected header element after clicking into submenu level 0 to have header type",
                SUBMENU_HEADER,
                header.type);
        assertEquals(
                "Expected 2nd element after clicking into submenu level 0 to be another submenu"
                        + " parent",
                MENU_ITEM_WITH_SUBMENU,
                mModelList.get(0).type);
        // Go back to the root level
        activateClickListener(header);
        verify(mDismissDialog, never()).run(); // Clicking into submenu and back should not dismiss
        // Verify correctness of model contents
        assertEquals(
                "Expected header section to be empty after returning to root",
                0,
                headerModelList.size());
        assertEquals("Expected root level to have 2 items", 2, mModelList.size());
        assertEquals(
                "Expected 1st element of root level to be submenu level 0",
                mSubmenuLevel0,
                mModelList.get(0));
        assertEquals(
                "Expected 2nd element of root level to be a menu item",
                mListItemWithoutModelClickCallback,
                mModelList.get(1));
        // Go into submenu 0 again
        activateClickListener(mSubmenuLevel0);
        // Should still have 1 header and 2 submenu item
        assertEquals(
                "Expected header section to still have 1 header for submenu level 0",
                1,
                headerModelList.size());
        assertEquals(
                "Expected content section to still have 2 elements for submenu level 0",
                2,
                mModelList.size());
        assertEquals(
                "Expected 1st element of header section to be submenu level 0 parent",
                SUBMENU_LEVEL_0,
                getTitle(headerModelList.get(0)));
        assertEquals(
                "Expected 1st element of content section to be submenu level 0 parent",
                SUBMENU_LEVEL_1,
                getTitle(mModelList.get(0)));
        assertEquals(
                "Expected 2nd element of content section to be submenu level 0 parent",
                SUBMENU_0_CHILD_1,
                getTitle(mModelList.get(1)));
        // Go into submenu 1
        activateClickListener(mSubmenuLevel1);
        assertEquals(
                "Expected there to be a header after navigating into submenu 1",
                1,
                headerModelList.size());
        assertEquals(
                "Expected there to be one child after navigating into submenu 1",
                1,
                mModelList.size());
        // Assert correctness of contents
        ListItem mSubmenuLevel1Header = headerModelList.get(0);
        assertEquals(
                "Expected header type to be SUBMENU_HEADER",
                SUBMENU_HEADER,
                mSubmenuLevel1Header.type);
        assertEquals(
                "Expected title to be submenu header 1",
                SUBMENU_LEVEL_1,
                getTitle(headerModelList.get(0)));
        assertEquals(
                "Expected content element to be correct child",
                mListItemWithModelClickCallback,
                mModelList.get(0));
    }

    @Test
    public void getItemList_withoutModelClickCallback_noClickCallbackAdded() {
        setupCallbacksRecursively(
                /* headerModelList= */ null,
                mModelList,
                mDismissDialog,
                /* flyoutHandler= */ null,
                /* drillDownOverrideValue= */ true);
        boolean hasClickListener =
                mListItemWithoutModelClickCallback.model.containsKey(CLICK_LISTENER);
        assertTrue(
                "Expected list item's click callback to not be set. Had click listener key ? "
                        + hasClickListener
                        + (hasClickListener
                                ? ", and click listener value = "
                                        + mListItemWithoutModelClickCallback.model.get(
                                                CLICK_LISTENER)
                                : ""),
                !hasClickListener
                        || mListItemWithoutModelClickCallback.model.get(CLICK_LISTENER) == null);
    }

    @Test
    public void getItemList_withModelClickCallback_dismissAdded() {
        setupCallbacksRecursively(
                /* headerModelList= */ null,
                mModelList,
                mDismissDialog,
                /* flyoutHandler= */ null,
                /* drillDownOverrideValue= */ true);
        mListItemWithModelClickCallback.model.get(CLICK_LISTENER).onClick(mListView);
        verify(mDismissDialog, times(1)).run();
    }

    @Test
    public void getItemList_submenuNavigation_noOneByOneDataChange() {
        setupCallbacksRecursively(
                /* headerModelList= */ null,
                mModelList,
                mDismissDialog,
                /* flyoutHandler= */ null,
                /* drillDownOverrideValue= */ true);
        mModelList.addObserver(mListObserver);
        // Click into submenu 0
        activateClickListener(mSubmenuLevel0);
        // Assert that list observer was called once with correct arguments
        verify(mListObserver, never()).onItemRangeRemoved(any(), anyInt(), anyInt());
        verify(mListObserver, never()).onItemMoved(any(), anyInt(), anyInt());
        verify(mListObserver, times(1)).onItemRangeChanged(mModelList, 0, 2, null);
        verify(mListObserver, times(1)).onItemRangeInserted(mModelList, 2, 1);
    }

    @Test
    public void getItemList_submenuFlyoutNavigation_hoverShowsFlyoutAfterDelay() {
        setupCallbacksRecursively(
                /* headerModelList= */ null,
                mModelList,
                mDismissDialog,
                mFlyoutHandler,
                /* drillDownOverrideValue= */ false);

        // Create the main menu popup window (level 0).
        List<Pair<@Nullable ListItem, Object>> dialogs = new ArrayList<>();
        dialogs.add(new Pair(null, new Object()));
        when(mFlyoutHandler.getFlyoutWindows()).thenReturn(dialogs);

        // Start hover on one of the items on the main menu (level 0).
        activateHoverListener(mSubmenuLevel0);

        // Verify that before the delay, no new window is added.
        verify(mFlyoutHandler, never()).addFlyoutWindow(any(), any());

        // Wait for the UI delay.
        waitForUiDelay();

        // Verify that the call to create a new popup (level 1) is called.
        verify(mFlyoutHandler).addFlyoutWindow(mSubmenuLevel0, mListView);
        dialogs.add(new Pair(mSubmenuLevel0, new Object()));

        // Hover on an item inside the level 1 popup for long enough.
        activateHoverListener(mSubmenuLevel1);
        waitForUiDelay();

        // Verify that the call to create another popup (level 2) is called.
        verify(mFlyoutHandler).addFlyoutWindow(mSubmenuLevel1, mListView);
    }

    @Test
    public void getItemList_submenuFlyoutNavigation_hoverOnNewItemClosesAllDescendentPopups() {
        setupCallbacksRecursively(
                /* headerModelList= */ null,
                mModelList,
                mDismissDialog,
                mFlyoutHandler,
                /* drillDownOverrideValue= */ false);

        List<Pair<@Nullable ListItem, Object>> dialogs = new ArrayList<>();
        when(mFlyoutHandler.getFlyoutWindows()).thenReturn(dialogs);

        // Create level 0, 1, and 2 popup windows.
        dialogs.add(new Pair(null, new Object())); // Level 0 popup.
        dialogs.add(new Pair(mSubmenuLevel0, new Object())); // Level 1 popup.
        dialogs.add(new Pair(mSubmenuLevel1, new Object())); // Level 2 popup.

        // Hover on a different item on the level 0 popup.
        activateHoverListener(mListItemWithoutModelClickCallback);
        waitForUiDelay();

        // Popups of level 1 and 2 should be removed.
        verify(mFlyoutHandler).removeFlyoutWindows(1);

        // Create level 0, 1, and 2 popup windows again.
        dialogs = new ArrayList<>();
        dialogs.add(new Pair(null, new Object())); // Level 0 popup.
        dialogs.add(new Pair(mSubmenuLevel0, new Object())); // Level 1 popup.
        dialogs.add(new Pair(mSubmenuLevel1, new Object())); // Level 2 popup.

        // Hover on a different item on the level 1 popup.
        activateHoverListener(mSubmenu0Child1);
        waitForUiDelay();

        // Level 2 popup should be removed, but level 1 popup should remain.
        verify(mFlyoutHandler).removeFlyoutWindows(2);
    }

    @Test
    public void getItemList_submenuFlyoutNavigation_hoverOnOriginalItemKeepsDirectChild() {
        setupCallbacksRecursively(
                /* headerModelList= */ null,
                mModelList,
                mDismissDialog,
                mFlyoutHandler,
                /* drillDownOverrideValue= */ false);

        List<Pair<@Nullable ListItem, Object>> dialogs = new ArrayList<>();
        when(mFlyoutHandler.getFlyoutWindows()).thenReturn(dialogs);

        // Create level 0, 1, and 2 popup windows.
        dialogs.add(new Pair(null, new Object())); // Level 0 popup.
        dialogs.add(new Pair(mSubmenuLevel0, new Object())); // Level 1 popup.
        dialogs.add(new Pair(mSubmenuLevel1, new Object())); // Level 2 popup.

        // Hover on the original item on the level 0 popup.
        activateHoverListener(mSubmenuLevel0);
        waitForUiDelay();

        // Level 2 popup should be removed, but level 1 popup should remain.
        verify(mFlyoutHandler).removeFlyoutWindows(2);
        dialogs.subList(2, dialogs.size()).clear();
    }

    @Test
    public void testHighlightPath_UpdatesOnHoverSequentially() {
        setupCallbacksRecursively(
                /* headerModelList= */ null,
                mModelList,
                mDismissDialog,
                mFlyoutHandler,
                /* drillDownOverrideValue= */ false);

        // Case 1: Hover root item 1 ("Submenu level 0")
        activateHoverListener(mSubmenuLevel0);
        assertEquals(
                "mSubmenuLevel0 should be highlighted",
                true,
                mSubmenuLevel0.model.get(IS_HIGHLIGHTED));
        assertEquals(
                "Sibling should not be highlighted",
                false,
                mListItemWithoutModelClickCallback.model.get(IS_HIGHLIGHTED));

        // Case 2: Hover a child ("Submenu level 1")
        activateHoverListener(mSubmenuLevel1);
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
        activateHoverListener(mSubmenu0Child1);
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
        activateHoverListener(mListItemWithModelClickCallback);
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
        activateHoverListener(mSubmenuLevel1);
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
        activateHoverListener(mListItemWithoutModelClickCallback);
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

    private void activateClickListener(ListItem item) {
        item.model.get(CLICK_LISTENER).onClick(mListView);
    }

    private void activateHoverListener(ListItem item) {
        item.model
                .get(HOVER_LISTENER)
                .onHover(
                        mListView,
                        MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 1.f, 1.f, 0));
    }

    private static CharSequence getTitle(ListItem item) {
        return item.model.get(TITLE);
    }

    private static void waitForUiDelay() {
        shadowOf(Looper.getMainLooper()).idle();
        ShadowLooper.runMainLooperOneTask();
    }
}
