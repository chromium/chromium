// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.view.View;
import android.widget.ListView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/** Unit tests for {@link ListMenuItemAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ListMenuItemAdapterUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;
    private ListView mParent;

    @Before
    public void setup() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mParent = new ListView(mActivity);
    }

    @Test
    public void testSubmenuAndHeaderItems_preserveClickListenerOnViewReuse() {
        AtomicInteger submenuClicks = new AtomicInteger();
        AtomicInteger headerClicks = new AtomicInteger();
        AtomicInteger delegateClicks = new AtomicInteger();

        ModelList modelList = new ModelList();
        modelList.add(
                new ListItem(
                        ListItemType.MENU_ITEM_WITH_SUBMENU,
                        new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                                .with(ListMenuItemProperties.TITLE, "Submenu")
                                .with(ListMenuItemProperties.ENABLED, true)
                                .with(
                                        ListMenuSubmenuItemProperties.SUBMENU_PROVIDER,
                                        () -> List.of())
                                .with(
                                        ListMenuItemProperties.CLICK_LISTENER,
                                        v -> submenuClicks.incrementAndGet())
                                .build()));
        modelList.add(
                new ListItem(
                        ListItemType.SUBMENU_HEADER,
                        new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                                .with(ListMenuItemProperties.TITLE, "Header")
                                .with(ListMenuItemProperties.ENABLED, true)
                                .with(
                                        ListMenuItemProperties.CLICK_LISTENER,
                                        v -> headerClicks.incrementAndGet())
                                .build()));

        ListMenuItemAdapter adapter =
                ListMenuUtils.createAdapter(
                        modelList, Set.of(), (model, view) -> delegateClicks.incrementAndGet());

        View submenuRow = adapter.getView(0, /* convertView= */ null, mParent);
        submenuRow = adapter.getView(0, submenuRow, mParent);
        assertTrue(submenuRow.performClick());
        assertEquals(1, submenuClicks.get());

        View headerRow = adapter.getView(1, /* convertView= */ null, mParent);
        headerRow = adapter.getView(1, headerRow, mParent);
        assertTrue(headerRow.performClick());
        assertEquals(1, headerClicks.get());

        assertEquals(0, delegateClicks.get());
    }

    @Test
    public void testCheckboxAndRadioItems_preserveClickListenerOnViewReuse() {
        AtomicInteger checkboxClicks = new AtomicInteger();
        AtomicInteger radioClicks = new AtomicInteger();
        AtomicInteger delegateClicks = new AtomicInteger();

        ModelList modelList = new ModelList();
        modelList.add(
                new ListItem(
                        ListItemType.MENU_ITEM_WITH_CHECKBOX,
                        new PropertyModel.Builder(ListMenuCheckItemProperties.ALL_KEYS)
                                .with(ListMenuItemProperties.TITLE, "Checkbox")
                                .with(ListMenuItemProperties.ENABLED, true)
                                .with(
                                        ListMenuItemProperties.CLICK_LISTENER,
                                        v -> checkboxClicks.incrementAndGet())
                                .build()));
        modelList.add(
                new ListItem(
                        ListItemType.MENU_ITEM_WITH_RADIO_BUTTON,
                        new PropertyModel.Builder(ListMenuRadioItemProperties.ALL_KEYS)
                                .with(ListMenuItemProperties.TITLE, "Radio")
                                .with(ListMenuItemProperties.ENABLED, true)
                                .with(
                                        ListMenuItemProperties.CLICK_LISTENER,
                                        v -> radioClicks.incrementAndGet())
                                .build()));

        ListMenuItemAdapter adapter =
                ListMenuUtils.createAdapter(
                        modelList, Set.of(), (model, view) -> delegateClicks.incrementAndGet());

        View checkboxRow = adapter.getView(0, /* convertView= */ null, mParent);
        checkboxRow = adapter.getView(0, checkboxRow, mParent);
        assertTrue(checkboxRow.performClick());
        assertEquals(1, checkboxClicks.get());

        View radioRow = adapter.getView(1, /* convertView= */ null, mParent);
        radioRow = adapter.getView(1, radioRow, mParent);
        assertTrue(radioRow.performClick());
        assertEquals(1, radioClicks.get());

        assertEquals(0, delegateClicks.get());
    }

    @Test
    public void testCheckboxAndRadioItems_fallBackToDelegateWhenClickListenerUnsetOrNull() {
        AtomicInteger checkboxListenerClicks = new AtomicInteger();
        AtomicInteger radioListenerClicks = new AtomicInteger();
        List<PropertyModel> delegatedModels = new ArrayList<>();

        PropertyModel checkboxUnsetModel =
                new PropertyModel.Builder(ListMenuCheckItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, "Checkbox Unset")
                        .with(ListMenuItemProperties.ENABLED, true)
                        .build();
        PropertyModel checkboxNullModel =
                new PropertyModel.Builder(ListMenuCheckItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, "Checkbox Null")
                        .with(ListMenuItemProperties.ENABLED, true)
                        .with(ListMenuItemProperties.CLICK_LISTENER, null)
                        .build();
        PropertyModel radioUnsetModel =
                new PropertyModel.Builder(ListMenuRadioItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, "Radio Unset")
                        .with(ListMenuItemProperties.ENABLED, true)
                        .build();
        PropertyModel radioNullModel =
                new PropertyModel.Builder(ListMenuRadioItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, "Radio Null")
                        .with(ListMenuItemProperties.ENABLED, true)
                        .with(ListMenuItemProperties.CLICK_LISTENER, null)
                        .build();

        ModelList modelList = new ModelList();
        modelList.add(
                new ListItem(
                        ListItemType.MENU_ITEM_WITH_CHECKBOX,
                        new PropertyModel.Builder(ListMenuCheckItemProperties.ALL_KEYS)
                                .with(ListMenuItemProperties.TITLE, "Checkbox With Listener")
                                .with(ListMenuItemProperties.ENABLED, true)
                                .with(
                                        ListMenuItemProperties.CLICK_LISTENER,
                                        v -> checkboxListenerClicks.incrementAndGet())
                                .build()));
        modelList.add(new ListItem(ListItemType.MENU_ITEM_WITH_CHECKBOX, checkboxUnsetModel));
        modelList.add(new ListItem(ListItemType.MENU_ITEM_WITH_CHECKBOX, checkboxNullModel));
        modelList.add(
                new ListItem(
                        ListItemType.MENU_ITEM_WITH_RADIO_BUTTON,
                        new PropertyModel.Builder(ListMenuRadioItemProperties.ALL_KEYS)
                                .with(ListMenuItemProperties.TITLE, "Radio With Listener")
                                .with(ListMenuItemProperties.ENABLED, true)
                                .with(
                                        ListMenuItemProperties.CLICK_LISTENER,
                                        v -> radioListenerClicks.incrementAndGet())
                                .build()));
        modelList.add(new ListItem(ListItemType.MENU_ITEM_WITH_RADIO_BUTTON, radioUnsetModel));
        modelList.add(new ListItem(ListItemType.MENU_ITEM_WITH_RADIO_BUTTON, radioNullModel));

        ListMenuItemAdapter adapter =
                ListMenuUtils.createAdapter(
                        modelList, Set.of(), (model, view) -> delegatedModels.add(model));

        View checkboxRow = adapter.getView(0, /* convertView= */ null, mParent);
        checkboxRow = adapter.getView(1, checkboxRow, mParent);
        assertTrue(checkboxRow.performClick());
        checkboxRow = adapter.getView(2, checkboxRow, mParent);
        assertTrue(checkboxRow.performClick());

        View radioRow = adapter.getView(3, /* convertView= */ null, mParent);
        radioRow = adapter.getView(4, radioRow, mParent);
        assertTrue(radioRow.performClick());
        radioRow = adapter.getView(5, radioRow, mParent);
        assertTrue(radioRow.performClick());

        assertEquals(0, checkboxListenerClicks.get());
        assertEquals(0, radioListenerClicks.get());
        assertEquals(
                List.of(checkboxUnsetModel, checkboxNullModel, radioUnsetModel, radioNullModel),
                delegatedModels);
    }

    @Test
    public void testMenuItem_rebindsDelegateWithNewItemOnViewReuse() {
        List<PropertyModel> delegatedModels = new ArrayList<>();
        PropertyModel firstModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, "First Item")
                        .with(ListMenuItemProperties.ENABLED, true)
                        .build();
        PropertyModel secondModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, "Second Item")
                        .with(ListMenuItemProperties.ENABLED, true)
                        .build();

        ModelList modelList = new ModelList();
        modelList.add(new ListItem(ListItemType.MENU_ITEM, firstModel));
        modelList.add(new ListItem(ListItemType.MENU_ITEM, secondModel));

        ListMenuItemAdapter adapter =
                ListMenuUtils.createAdapter(
                        modelList, Set.of(), (model, view) -> delegatedModels.add(model));

        View row = adapter.getView(0, /* convertView= */ null, mParent);
        assertTrue(row.performClick());

        row = adapter.getView(1, row, mParent);
        assertTrue(row.performClick());

        assertEquals(List.of(firstModel, secondModel), delegatedModels);
    }
}
