// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests to ensure/validate SimpleRecyclerViewAdapter behavior.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SimpleRecyclerViewAdapterTest {
    private static final PropertyModel.WritableIntPropertyKey INT_PROPERTY =
            new PropertyModel.WritableIntPropertyKey();
    private static final Integer VIEW_TYPE_1 = 1;
    private static final Integer VIEW_TYPE_2 = 2;
    private static final Integer VIEW_TYPE_3 = 3;

    private ModelList mModelList;
    private PropertyModel mModel;

    // Mockito Spies allow us to intercept calls to parent class.
    private SimpleRecyclerViewAdapter mSpyAdapter;

    @Mock
    View mMockView;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mModel = new PropertyModel(INT_PROPERTY);
        mModelList = new ModelList();

        // Note: this behaves both like a mock and a real object.
        // Methods calls can be mocked or tracked to validate class behavior.
        mSpyAdapter = Mockito.mock(SimpleRecyclerViewAdapter.class,
                Mockito.withSettings()
                        .useConstructor(mModelList)
                        .defaultAnswer(Mockito.CALLS_REAL_METHODS));

        mSpyAdapter.registerType(VIEW_TYPE_1, parent -> mMockView, (m, v, p) -> {});
        mSpyAdapter.registerType(VIEW_TYPE_2, parent -> mMockView, (m, v, p) -> {});
        mSpyAdapter.registerType(VIEW_TYPE_3, parent -> mMockView, (m, v, p) -> {});
    }

    @Test
    public void testObserver_listModelItemsAdded() {
        mModelList.add(new ModelListAdapter.ListItem(VIEW_TYPE_1, mModel));
        verify(mSpyAdapter, times(1)).notifyItemRangeInserted(0, 1);
        mModelList.add(new ModelListAdapter.ListItem(VIEW_TYPE_2, mModel));
        verify(mSpyAdapter, times(1)).notifyItemRangeInserted(1, 1);
    }

    @Test
    public void testObserver_listModelItemsAddedInBatch() {
        mModelList.add(new ModelListAdapter.ListItem(VIEW_TYPE_1, mModel));
        verify(mSpyAdapter, times(1)).notifyItemRangeInserted(0, 1);
        List<ModelListAdapter.ListItem> items = new ArrayList<>();
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_1, mModel));
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_2, mModel));
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_3, mModel));
        mModelList.addAll(items);
        verify(mSpyAdapter, times(1)).notifyItemRangeInserted(1, 3);
    }

    @Test
    public void testObserver_listModelItemsSet() {
        List<ModelListAdapter.ListItem> items = new ArrayList<>();
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_1, mModel));
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_2, mModel));
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_3, mModel));
        mModelList.set(items);
        verify(mSpyAdapter, times(1)).notifyItemRangeInserted(0, 3);
    }

    @Test
    public void testObserver_listModelItemsRemove() {
        List<ModelListAdapter.ListItem> items = new ArrayList<>();
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_1, mModel));
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_2, mModel));
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_3, mModel));
        mModelList.set(items);
        verify(mSpyAdapter, times(1)).notifyItemRangeInserted(0, 3);
        mModelList.removeRange(0, 2);
        verify(mSpyAdapter, times(1)).notifyItemRangeRemoved(0, 2);
    }

    @Test
    public void testObserver_listModelItemsClear() {
        List<ModelListAdapter.ListItem> items = new ArrayList<>();
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_1, mModel));
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_2, mModel));
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_3, mModel));
        mModelList.set(items);
        verify(mSpyAdapter, times(1)).notifyItemRangeInserted(0, 3);
        mModelList.clear();
        verify(mSpyAdapter, times(1)).notifyItemRangeRemoved(0, 3);
    }

    @Test
    public void testObserver_listModelItemMoved() {
        List<ModelListAdapter.ListItem> items = new ArrayList<>();
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_1, mModel));
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_2, mModel));
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_3, mModel));
        mModelList.set(items);
        verify(mSpyAdapter, times(1)).notifyItemRangeInserted(0, 3);

        mModelList.move(1, 2);
        verify(mSpyAdapter, times(1)).notifyItemMoved(1, 2);
    }

    @Test
    public void testObserver_listModelItemUpdated() {
        List<ModelListAdapter.ListItem> items = new ArrayList<>();
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_1, mModel));
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_2, mModel));
        items.add(new ModelListAdapter.ListItem(VIEW_TYPE_3, mModel));
        mModelList.set(items);
        verify(mSpyAdapter, times(1)).notifyItemRangeInserted(0, 3);

        mModelList.update(1, new ModelListAdapter.ListItem(VIEW_TYPE_2, mModel));
        verify(mSpyAdapter, times(1)).notifyItemRangeChanged(1, 1);
    }
}
