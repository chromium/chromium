// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.hierarchicalmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import android.graphics.Rect;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ListView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController.AccessibilityListObserver;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController.SubmenuHeaderFactory;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link HierarchicalMenuControllerUnitTest}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HierarchicalMenuControllerUnitTest {

    private static final int TEST_MENU_ITEM_ID = 3; // Arbitrary int for testing
    private static final String TOP_LEVEL_ITEM = "Top level item";
    private static final String SUBMENU_LEVEL_0 = "Submenu level 0";
    private static final String SUBMENU_0_CHILD_1 = "Submenu 0 child 1";
    private static final String SUBMENU_LEVEL_1 = "Submenu level 1";
    private static final String SUBMENU_1_CHILD_0 = "Submenu 1 child 0";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OnClickListener mItemClickListener;
    @Mock private Runnable mDismissDialog;
    @Mock private View mParentView;
    @Mock private ListView mHeaderListView;
    @Mock private ListView mListView;
    @Mock private ListObservable.ListObserver<Void> mListObserver;

    private final ModelList mHeaderModelList = new ModelList();
    private final ModelList mModelList = new ModelList();
    private ListItem mListItemWithModelClickCallback;
    private ListItem mSubmenuLevel1;
    private ListItem mSubmenu0Child1;
    private ListItem mSubmenuLevel0;
    private ListItem mListItemWithoutModelClickCallback;
    private HierarchicalMenuController mController;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();

        HierarchicalMenuKeyProvider keyProvider = HierarchicalMenuTestUtils.createKeyProvider();
        SubmenuHeaderFactory headerFactory =
                (clickedItem, backRunnable) -> {
                    PropertyModel.Builder builder =
                            new PropertyModel.Builder(ALL_SUBMENU_ITEM_KEYS);
                    HierarchicalMenuController.populateDefaultHeaderProperties(
                            builder, keyProvider, clickedItem.model.get(TITLE), backRunnable);
                    return new ListItem(MENU_ITEM_SUBMENU_HEADER, builder.build());
                };

        mController = new HierarchicalMenuController(context, keyProvider, headerFactory);

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
        mModelList.add(mSubmenuLevel0);

        // Add an item with no click callback
        mListItemWithoutModelClickCallback =
                new ListItem(
                        MENU_ITEM,
                        new PropertyModel.Builder(ALL_MENU_ITEM_KEYS)
                                .with(TITLE, TOP_LEVEL_ITEM)
                                .with(ENABLED, true)
                                .with(MENU_ITEM_ID, TEST_MENU_ITEM_ID)
                                .with(IS_HIGHLIGHTED, false)
                                .build());
        mModelList.add(mListItemWithoutModelClickCallback);

        when(mListView.getContext()).thenReturn(context);
        when(mParentView.getContext()).thenReturn(context);
    }

    @Test
    public void getItemList_submenuNavigation_noStaticHeader() {
        mController.setupCallbacksRecursively(
                /* headerModelList= */ null, mModelList, mDismissDialog);
        // Click into submenu 0
        activateClickListener(mSubmenuLevel0);
        assertEquals(
                "Expected submenu level 0 to have 3 items (1 header and 2 children)",
                3,
                mModelList.size());
        ListItem header = mModelList.get(0);
        assertEquals(
                "Expected 1st element after clicking into submenu level 0 to have header type",
                MENU_ITEM_SUBMENU_HEADER,
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
                MENU_ITEM_SUBMENU_HEADER,
                mModelList.get(0).type);
        assertEquals(
                "Expected 2nd element to be correct child",
                mListItemWithModelClickCallback,
                mModelList.get(1));
    }

    @Test
    public void getItemList_submenuNavigation_withStaticHeader() {
        // Begin test
        mController.setupCallbacksRecursively(mHeaderModelList, mModelList, mDismissDialog);
        // Click into submenu 0
        activateClickListener(mSubmenuLevel0);
        assertEquals(
                "Expected header section to have 1 header for submenu level 0",
                1,
                mHeaderModelList.size());
        assertEquals(
                "Expected content section to have 2 children for submenu level 0",
                2,
                mModelList.size());
        ListItem header = mHeaderModelList.get(0);
        assertEquals(
                "Expected header element after clicking into submenu level 0 to have header type",
                MENU_ITEM_SUBMENU_HEADER,
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
                mHeaderModelList.size());
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
                mHeaderModelList.size());
        assertEquals(
                "Expected content section to still have 2 elements for submenu level 0",
                2,
                mModelList.size());
        assertEquals(
                "Expected 1st element of header section to be submenu level 0 parent",
                SUBMENU_LEVEL_0,
                getTitle(mHeaderModelList.get(0)));
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
                mHeaderModelList.size());
        assertEquals(
                "Expected there to be one child after navigating into submenu 1",
                1,
                mModelList.size());
        // Assert correctness of contents
        ListItem mSubmenuLevel1Header = mHeaderModelList.get(0);
        assertEquals(
                "Expected header type to be SUBMENU_HEADER",
                MENU_ITEM_SUBMENU_HEADER,
                mSubmenuLevel1Header.type);
        assertEquals(
                "Expected title to be submenu header 1",
                SUBMENU_LEVEL_1,
                getTitle(mHeaderModelList.get(0)));
        assertEquals(
                "Expected content element to be correct child",
                mListItemWithModelClickCallback,
                mModelList.get(0));
    }

    @Test
    public void getItemList_withoutModelClickCallback_noClickCallbackAdded() {
        mController.setupCallbacksRecursively(
                /* headerModelList= */ null, mModelList, mDismissDialog);
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
        mController.setupCallbacksRecursively(
                /* headerModelList= */ null, mModelList, mDismissDialog);
        mListItemWithModelClickCallback.model.get(CLICK_LISTENER).onClick(mListView);
        verify(mDismissDialog, times(1)).run();
    }

    @Test
    public void getItemList_submenuNavigation_noOneByOneDataChange() {
        mController.setupCallbacksRecursively(
                /* headerModelList= */ null, mModelList, mDismissDialog);
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
    public void submenuNavigation_a11y_withHeader() {
        AccessibilityListObserver observer =
                mController
                .new AccessibilityListObserver(
                        mParentView, mHeaderListView, mListView, mHeaderModelList, mModelList);
        mHeaderModelList.addObserver(observer);
        mModelList.addObserver(observer);

        mController.setupCallbacksRecursively(mHeaderModelList, mModelList, mDismissDialog);

        // Click into submenu 0
        activateClickListener(mSubmenuLevel0);

        // Assert correct a11y behavior
        verify(mParentView).setAccessibilityPaneTitle(SUBMENU_LEVEL_0);
        InOrder inOrder = inOrder(mParentView, mHeaderListView, mListView);
        inOrder.verify(mListView).setSelection(0);
        inOrder.verify(mHeaderListView).setSelection(0);
        inOrder.verify(mParentView).requestFocus();
    }

    @Test
    public void submenuNavigation_a11y_noHeader() {
        AccessibilityListObserver observer =
                mController
                .new AccessibilityListObserver(
                        mParentView,
                        /* headerView= */ null,
                        mListView,
                        mHeaderModelList,
                        mModelList);
        mModelList.addObserver(observer);

        mController.setupCallbacksRecursively(
                /* headerModelList= */ null, mModelList, mDismissDialog);

        // Click into submenu 0
        activateClickListener(mSubmenuLevel0);

        // Assert correct a11y behavior
        verify(mParentView).setAccessibilityPaneTitle(SUBMENU_LEVEL_0);
        InOrder inOrder = inOrder(mParentView, mListView);
        inOrder.verify(mListView).setSelection(0);
        inOrder.verify(mParentView).requestFocus();
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

    @Test
    public void possibleToFitFlyout_CorrectlyCalculatesFitForVariousLayouts() {
        int windowWidth = 1000;
        int minSpacing = 48;
        int minMargin = 48;
        int menuMaxWidth = 500;

        Rect mainPopupRect = new Rect(450, 0, 550, 100);
        assertFalse(
                "Should return false when there is not enough space to either side.",
                HierarchicalMenuController.possibleToFitFlyout(
                        mainPopupRect, windowWidth, minSpacing, minMargin, menuMaxWidth));

        mainPopupRect = new Rect(50, 0, 150, 100);
        assertTrue(
                "Should return true when there is enough space to the right.",
                HierarchicalMenuController.possibleToFitFlyout(
                        mainPopupRect, windowWidth, minSpacing, minMargin, menuMaxWidth));

        mainPopupRect = new Rect(850, 0, 950, 100);
        assertTrue(
                "Should return true when there is enough space to the right.",
                HierarchicalMenuController.possibleToFitFlyout(
                        mainPopupRect, windowWidth, minSpacing, minMargin, menuMaxWidth));
    }

    private void triggerHoverEnter(ListItem item, int level, List<ListItem> path) {
        mController.handleHoverEvent(createHoverEnterEvent(), item, mListView, level, path);
    }

    private MotionEvent createHoverEnterEvent() {
        return MotionEvent.obtain(0, 0, MotionEvent.ACTION_HOVER_ENTER, 1.f, 1.f, 0);
    }

    private void activateClickListener(ListItem item) {
        item.model.get(CLICK_LISTENER).onClick(mListView);
    }

    private static CharSequence getTitle(ListItem item) {
        return item.model.get(TITLE);
    }
}
