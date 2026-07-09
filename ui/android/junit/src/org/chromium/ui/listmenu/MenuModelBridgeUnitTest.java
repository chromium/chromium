// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;

import java.util.Collection;
import java.util.List;

/** Tests for {@link MenuModelBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MenuModelBridgeUnitTest {
    private MenuModelBridge mMenuModelBridge;

    @Before
    public void setUp() {
        mMenuModelBridge = new MenuModelBridge(0L);
    }

    @Test
    @SmallTest
    public void testAddCommand() {
        mMenuModelBridge.addCommand("Test Command", null, true, 0);
        List<ListItem> items = mMenuModelBridge.getListItems();
        assertEquals(1, items.size());
        ListItem item = items.get(0);
        assertEquals(ListItemType.MENU_ITEM, item.type);
        Collection<PropertyKey> keys = item.model.getAllProperties();
        assertTrue(keys.contains(ListMenuItemProperties.TITLE));
    }

    @Test
    @SmallTest
    public void testAddCheck() {
        mMenuModelBridge.addCheck("Test Check", true, true, 0);
        List<ListItem> items = mMenuModelBridge.getListItems();
        assertEquals(1, items.size());
        ListItem item = items.get(0);
        assertEquals(ListItemType.MENU_ITEM_WITH_CHECKBOX, item.type);
        Collection<PropertyKey> keys = item.model.getAllProperties();
        assertTrue(keys.contains(ListMenuItemProperties.TITLE));
    }

    @Test
    @SmallTest
    public void testAddRadioButton() {
        mMenuModelBridge.addRadioButton("Test Radio", true, true, 0);
        List<ListItem> items = mMenuModelBridge.getListItems();
        assertEquals(1, items.size());
        ListItem item = items.get(0);
        assertEquals(ListItemType.MENU_ITEM_WITH_RADIO_BUTTON, item.type);
        Collection<PropertyKey> keys = item.model.getAllProperties();
        assertTrue(keys.contains(ListMenuItemProperties.TITLE));
    }

    @Test
    @SmallTest
    public void testAddSubmenu() {
        MenuModelBridge submenuBridge = new MenuModelBridge(0L);
        mMenuModelBridge.addSubmenu("Test Submenu", null, true, submenuBridge);
        List<ListItem> items = mMenuModelBridge.getListItems();
        assertEquals(1, items.size());
        ListItem item = items.get(0);
        assertEquals(ListItemType.MENU_ITEM_WITH_SUBMENU, item.type);
        Collection<PropertyKey> keys = item.model.getAllProperties();
        assertTrue(keys.contains(ListMenuItemProperties.TITLE));
    }

    @Test
    @SmallTest
    public void testAddDividerHasSectionDividerKeys() {
        mMenuModelBridge.addDivider();
        List<ListItem> items = mMenuModelBridge.getListItems();
        assertEquals(1, items.size());
        ListItem item = items.get(0);
        assertEquals(ListItemType.DIVIDER, item.type);
        Collection<PropertyKey> keys = item.model.getAllProperties();
        assertTrue(keys.contains(ListSectionDividerProperties.LEFT_PADDING_DIMEN_ID));
        assertTrue(keys.contains(ListSectionDividerProperties.RIGHT_PADDING_DIMEN_ID));
        assertTrue(keys.contains(ListSectionDividerProperties.COLOR_ID));
    }
}
