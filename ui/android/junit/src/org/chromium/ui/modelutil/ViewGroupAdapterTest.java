// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.content.Context;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.MVCListAdapter.ViewBuilder;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

import java.util.ArrayList;
import java.util.List;

/** Tests to ensure/validate ViewGroupAdapter behavior. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ViewGroupAdapterTest {
    private static final PropertyModel.WritableObjectPropertyKey<String> TEXT_PROPERTY =
            new PropertyModel.WritableObjectPropertyKey<String>();
    private static final int VIEW_TYPE = 1;
    private static final ViewBuilder<ViewGroup> VIEW_BUILDER = parent -> parent;
    private static final ViewBinder<PropertyModel, ViewGroup, PropertyKey> VIEW_BINDER =
            (model, view, propertyKey) -> {
                if (TEXT_PROPERTY.equals(propertyKey)) {
                    view.setContentDescription(model.get(TEXT_PROPERTY));
                }
            };

    private final Context mContext = RuntimeEnvironment.application;

    private ModelList mModelList;
    private LinearLayout mLinearLayout;
    private ViewGroupAdapter mAdapter;

    @Before
    public void setUp() {
        mModelList = new ModelList();

        mLinearLayout = new LinearLayout(mContext);
        mAdapter =
                new ViewGroupAdapter.Builder(mLinearLayout, mModelList)
                        .registerType(VIEW_TYPE, VIEW_BUILDER, VIEW_BINDER)
                        .build();
    }

    @After
    public void tearDown() {
        mAdapter.destroy();
    }

    @Test
    public void testEmpty() {
        Assert.assertEquals(List.of(), getChildViewTexts());
    }

    @Test
    public void testInitItems() {
        mAdapter.destroy();

        mModelList.add(new ListItem(VIEW_TYPE, createModel("a")));
        mModelList.add(new ListItem(VIEW_TYPE, createModel("b")));

        // Initialize the adapter with the non-empty model.
        mAdapter =
                new ViewGroupAdapter.Builder(mLinearLayout, mModelList)
                        .registerType(VIEW_TYPE, VIEW_BUILDER, VIEW_BINDER)
                        .build();

        Assert.assertEquals(List.of("a", "b"), getChildViewTexts());
    }

    @Test
    public void testAddItems() {
        mModelList.add(new ListItem(VIEW_TYPE, createModel("a")));
        mModelList.add(new ListItem(VIEW_TYPE, createModel("c")));
        mModelList.add(1, new ListItem(VIEW_TYPE, createModel("b")));

        Assert.assertEquals(List.of("a", "b", "c"), getChildViewTexts());
    }

    @Test
    public void testRemoveItems() {
        mModelList.add(new ListItem(VIEW_TYPE, createModel("a")));
        mModelList.add(new ListItem(VIEW_TYPE, createModel("b")));
        mModelList.add(new ListItem(VIEW_TYPE, createModel("c")));
        mModelList.removeAt(1);

        Assert.assertEquals(List.of("a", "c"), getChildViewTexts());
    }

    @Test
    public void testChangeItems() {
        mModelList.add(new ListItem(VIEW_TYPE, createModel("a")));
        mModelList.add(new ListItem(VIEW_TYPE, createModel("b")));
        mModelList.add(new ListItem(VIEW_TYPE, createModel("c")));
        mModelList.get(1).model.set(TEXT_PROPERTY, "B"); // Change the item
        mModelList.update(2, new ListItem(VIEW_TYPE, createModel("C"))); // Change the list

        Assert.assertEquals(List.of("a", "B", "C"), getChildViewTexts());
    }

    @Test
    public void testMoveItems() {
        mModelList.add(new ListItem(VIEW_TYPE, createModel("a")));
        mModelList.add(new ListItem(VIEW_TYPE, createModel("b")));
        mModelList.add(new ListItem(VIEW_TYPE, createModel("c")));
        mModelList.move(1, 2);

        Assert.assertEquals(List.of("a", "c", "b"), getChildViewTexts());
    }

    @Test
    public void testDuplicatedViewType() {
        Assert.assertThrows(
                Throwable.class,
                () -> {
                    // Registering the same type twice throws an exception.
                    new ViewGroupAdapter.Builder(mLinearLayout, mModelList)
                            .registerType(VIEW_TYPE, VIEW_BUILDER, VIEW_BINDER)
                            .registerType(VIEW_TYPE, VIEW_BUILDER, VIEW_BINDER)
                            .build();
                });
    }

    @Test
    public void testUnknownViewType() {
        Assert.assertThrows(
                Throwable.class,
                () -> {
                    // Using an item with unknown type throws an exception.
                    mModelList.add(new ListItem(12345, createModel("x")));
                });
    }

    @Test
    public void testDestroy() {
        mModelList.add(new ListItem(VIEW_TYPE, createModel("a")));
        mModelList.add(new ListItem(VIEW_TYPE, createModel("b")));

        Assert.assertEquals(List.of("a", "b"), getChildViewTexts());

        // On destroying the adapter, all views are removed.
        mAdapter.destroy();
        Assert.assertEquals(List.of(), getChildViewTexts());

        // Changes to the model are ignored after destroying the adapter.
        mModelList.add(new ListItem(VIEW_TYPE, createModel("c")));
        Assert.assertEquals(List.of(), getChildViewTexts());

        // Restore ViewGroupAdapter to avoid problems in tearDown.
        mAdapter =
                new ViewGroupAdapter.Builder(mLinearLayout, mModelList)
                        .registerType(VIEW_TYPE, VIEW_BUILDER, VIEW_BINDER)
                        .build();
    }

    private PropertyModel createModel(String text) {
        PropertyModel model = new PropertyModel(TEXT_PROPERTY);
        model.set(TEXT_PROPERTY, text);
        return model;
    }

    private List<String> getChildViewTexts() {
        int n = mLinearLayout.getChildCount();
        List<String> texts = new ArrayList<>();
        for (int i = 0; i < n; i++) {
            texts.add(mLinearLayout.getChildAt(i).getContentDescription().toString());
        }
        return texts;
    }
}
