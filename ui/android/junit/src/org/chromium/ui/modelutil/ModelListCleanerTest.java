// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import static org.junit.Assert.assertEquals;

import static org.chromium.ui.modelutil.ModelListCleaner.destroyAndClearAllRows;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.concurrent.atomic.AtomicInteger;

/** Tests to ensure/validate ModelListCleaner behavior. */
@RunWith(BaseRobolectricTestRunner.class)
public class ModelListCleanerTest {
    private static final Integer VIEW_TYPE = 0;
    private static final PropertyModel.WritableObjectPropertyKey<Destroyable> DESTROYABLE_0 =
            new PropertyModel.WritableObjectPropertyKey<>();
    private static final PropertyModel.WritableObjectPropertyKey<Destroyable> DESTROYABLE_1 =
            new PropertyModel.WritableObjectPropertyKey<>();
    private MVCListAdapter.ModelList mModelList;

    @Before
    public void setUp() {
        mModelList = new ModelListAdapter.ModelList();
    }

    @Test
    public void testDestroy() {
        AtomicInteger totalRuns = new AtomicInteger();
        Destroyable destroyable = totalRuns::getAndIncrement;

        PropertyModel.Builder builder = new PropertyModel.Builder(DESTROYABLE_0);
        builder.with(DESTROYABLE_0, destroyable);
        PropertyModel model = builder.build();

        mModelList.add(new ModelListAdapter.ListItem(VIEW_TYPE, model));
        mModelList.add(new ModelListAdapter.ListItem(VIEW_TYPE, model));
        mModelList.add(new ModelListAdapter.ListItem(VIEW_TYPE, model));

        destroyAndClearAllRows(mModelList, DESTROYABLE_0);
        assertEquals(3, totalRuns.get());
    }

    @Test
    public void testIgnoreNoDestroyable() {
        AtomicInteger totalRuns = new AtomicInteger();
        Destroyable destroyable = totalRuns::getAndIncrement;

        PropertyModel.Builder builder = new PropertyModel.Builder(DESTROYABLE_0);
        builder.with(DESTROYABLE_0, destroyable);
        PropertyModel model = builder.build();

        mModelList.add(new ModelListAdapter.ListItem(VIEW_TYPE, model));
        mModelList.add(new ModelListAdapter.ListItem(VIEW_TYPE, model));
        mModelList.add(new ModelListAdapter.ListItem(VIEW_TYPE, new PropertyModel()));

        destroyAndClearAllRows(mModelList, DESTROYABLE_0);
        assertEquals(2, totalRuns.get());
    }

    @Test
    public void testMultipleDestroyables() {
        AtomicInteger totalRuns = new AtomicInteger();
        Destroyable destroyable = totalRuns::getAndIncrement;

        PropertyModel.Builder builder = new PropertyModel.Builder(DESTROYABLE_0);
        builder.with(DESTROYABLE_0, destroyable);
        PropertyModel model = builder.build();

        builder = new PropertyModel.Builder(DESTROYABLE_1);
        builder.with(DESTROYABLE_1, destroyable);
        PropertyModel model1 = builder.build();

        mModelList.add(new ModelListAdapter.ListItem(VIEW_TYPE, model));
        mModelList.add(new ModelListAdapter.ListItem(VIEW_TYPE, model1));
        mModelList.add(new ModelListAdapter.ListItem(VIEW_TYPE, new PropertyModel()));
        mModelList.add(new ModelListAdapter.ListItem(VIEW_TYPE, new PropertyModel()));

        destroyAndClearAllRows(mModelList, DESTROYABLE_0, DESTROYABLE_1);
        assertEquals(2, totalRuns.get());
    }
}
