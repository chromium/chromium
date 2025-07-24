// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM_WITH_SUBMENU;
import static org.chromium.ui.listmenu.ListItemType.SUBMENU_HEADER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;
import static org.chromium.ui.listmenu.ListMenuUtils.setupCallbacksRecursively;

import android.view.View.OnClickListener;
import android.widget.ListView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

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

    @Mock private Callback<Integer> mClickCallback;
    @Mock private OnClickListener mItemClickListener;
    @Mock private Runnable mDismissDialog;
    @Mock private ListView mListView;

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
                                .build());

        mSubmenuLevel1 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, SUBMENU_LEVEL_1)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(mListItemWithModelClickCallback))
                                .build());

        mSubmenu0Child1 =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                .with(TITLE, SUBMENU_0_CHILD_1)
                                .with(ENABLED, true)
                                .with(MENU_ITEM_ID, TEST_MENU_ITEM_ID)
                                .build());
        mSubmenuLevel0 =
                new ListItem(
                        MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                                .with(TITLE, SUBMENU_LEVEL_0)
                                .with(ENABLED, true)
                                .with(SUBMENU_ITEMS, List.of(mSubmenuLevel1, mSubmenu0Child1))
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
                                .build());
        mModelList.add(mListItemWithoutModelClickCallback);
    }

    @Test
    public void getItemList_submenuNavigation_noStaticHeader() {
        setupCallbacksRecursively(/* headerModelList= */ null, mModelList, mDismissDialog);
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
        setupCallbacksRecursively(headerModelList, mModelList, mDismissDialog);
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
        setupCallbacksRecursively(/* headerModelList= */ null, mModelList, mDismissDialog);
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
        setupCallbacksRecursively(/* headerModelList= */ null, mModelList, mDismissDialog);
        mListItemWithModelClickCallback.model.get(CLICK_LISTENER).onClick(mListView);
        verify(mClickCallback, never()).onResult(any());
        verify(mDismissDialog, times(1)).run();
    }

    private void activateClickListener(ListItem item) {
        item.model.get(CLICK_LISTENER).onClick(mListView);
    }

    private static CharSequence getTitle(ListItem item) {
        return item.model.get(TITLE);
    }
}
