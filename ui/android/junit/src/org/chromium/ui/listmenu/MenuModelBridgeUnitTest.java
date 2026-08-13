// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.view.Menu;

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
        mMenuModelBridge.addCommand(0, -1, "Test Command", null, true, 0);
        List<ListItem> items = mMenuModelBridge.getListItems();
        assertEquals(1, items.size());
        ListItem item = items.get(0);
        assertEquals(ListItemType.MENU_ITEM, item.type);
        Collection<PropertyKey> keys = item.model.getAllProperties();
        assertTrue(keys.contains(ListMenuItemProperties.TITLE));
        assertEquals(Menu.CATEGORY_ALTERNATIVE, item.model.get(ListMenuItemProperties.ORDER));
    }

    @Test
    @SmallTest
    public void testAddCommand_customOrder() {
        // 35003 == IDC_PRINT (chrome/app/chrome_command_ids.h).
        mMenuModelBridge.addCommand(35003, 100, "Print", null, true, 0);
        ListItem item = mMenuModelBridge.getListItems().get(0);
        assertEquals(100, item.model.get(ListMenuItemProperties.ORDER));
        assertEquals(35003, item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @SmallTest
    public void testAddCheck() {
        mMenuModelBridge.addCheck(101, -1, "Test Check", true, true, 0);
        List<ListItem> items = mMenuModelBridge.getListItems();
        assertEquals(1, items.size());
        ListItem item = items.get(0);
        assertEquals(ListItemType.MENU_ITEM_WITH_CHECKBOX, item.type);
        Collection<PropertyKey> keys = item.model.getAllProperties();
        assertTrue(keys.contains(ListMenuItemProperties.TITLE));
        assertEquals(Menu.CATEGORY_ALTERNATIVE, item.model.get(ListMenuItemProperties.ORDER));
        assertEquals(101, item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @SmallTest
    public void testAddCheck_customOrder() {
        mMenuModelBridge.addCheck(101, 50, "Test Check", true, true, 0);
        ListItem item = mMenuModelBridge.getListItems().get(0);
        assertEquals(50, item.model.get(ListMenuItemProperties.ORDER));
        assertEquals(101, item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @SmallTest
    public void testAddRadioButton() {
        mMenuModelBridge.addRadioButton(102, -1, "Test Radio", true, true, 0);
        List<ListItem> items = mMenuModelBridge.getListItems();
        assertEquals(1, items.size());
        ListItem item = items.get(0);
        assertEquals(ListItemType.MENU_ITEM_WITH_RADIO_BUTTON, item.type);
        Collection<PropertyKey> keys = item.model.getAllProperties();
        assertTrue(keys.contains(ListMenuItemProperties.TITLE));
        assertEquals(Menu.CATEGORY_ALTERNATIVE, item.model.get(ListMenuItemProperties.ORDER));
        assertEquals(102, item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @SmallTest
    public void testAddRadioButton_customOrder() {
        mMenuModelBridge.addRadioButton(102, 60, "Test Radio", true, true, 0);
        ListItem item = mMenuModelBridge.getListItems().get(0);
        assertEquals(60, item.model.get(ListMenuItemProperties.ORDER));
        assertEquals(102, item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @SmallTest
    public void testAddSubmenu() {
        MenuModelBridge submenuBridge = new MenuModelBridge(0L);
        mMenuModelBridge.addSubmenu(103, -1, "Test Submenu", null, true, submenuBridge);
        List<ListItem> items = mMenuModelBridge.getListItems();
        assertEquals(1, items.size());
        ListItem item = items.get(0);
        assertEquals(ListItemType.MENU_ITEM_WITH_SUBMENU, item.type);
        Collection<PropertyKey> keys = item.model.getAllProperties();
        assertTrue(keys.contains(ListMenuItemProperties.TITLE));
        assertEquals(Menu.CATEGORY_ALTERNATIVE, item.model.get(ListMenuItemProperties.ORDER));
        assertEquals(103, item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @SmallTest
    public void testAddSubmenu_customOrder() {
        MenuModelBridge submenuBridge = new MenuModelBridge(0L);
        mMenuModelBridge.addSubmenu(103, 70, "Test Submenu", null, true, submenuBridge);
        ListItem item = mMenuModelBridge.getListItems().get(0);
        assertEquals(70, item.model.get(ListMenuItemProperties.ORDER));
        assertEquals(103, item.model.get(ListMenuItemProperties.MENU_ITEM_ID));
    }

    @Test
    @SmallTest
    public void testAddDividerHasSectionDividerKeys() {
        mMenuModelBridge.addDivider(-1);
        List<ListItem> items = mMenuModelBridge.getListItems();
        assertEquals(1, items.size());
        ListItem item = items.get(0);
        assertEquals(ListItemType.DIVIDER, item.type);
        Collection<PropertyKey> keys = item.model.getAllProperties();
        assertTrue(keys.contains(ListSectionDividerProperties.LEFT_PADDING_DIMEN_ID));
        assertTrue(keys.contains(ListSectionDividerProperties.RIGHT_PADDING_DIMEN_ID));
        assertTrue(keys.contains(ListSectionDividerProperties.COLOR_ID));
        assertEquals(Menu.CATEGORY_ALTERNATIVE, item.model.get(ListMenuItemProperties.ORDER));
    }

    @Test
    @SmallTest
    public void testAddDivider_customOrder() {
        mMenuModelBridge.addDivider(80);
        ListItem item = mMenuModelBridge.getListItems().get(0);
        assertEquals(80, item.model.get(ListMenuItemProperties.ORDER));
    }
}
