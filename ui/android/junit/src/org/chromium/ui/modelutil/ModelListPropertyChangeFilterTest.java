// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Unit tests for {@link ModelListPropertyChangeFilter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ModelListPropertyChangeFilterTest {
    private static final WritableBooleanPropertyKey PROPERTY_FOO = new WritableBooleanPropertyKey();
    private static final WritableBooleanPropertyKey PROPERTY_BAR = new WritableBooleanPropertyKey();

    private int mCallbackCounter;

    private void onPropertyChange() {
        mCallbackCounter++;
    }

    @Test
    public void testSetProperty() {
        ModelList modelList = new ModelList();
        PropertyModel propertyModel = new PropertyModel(PROPERTY_FOO, PROPERTY_BAR);
        modelList.add(new ListItem(/* type= */ 0, propertyModel));

        ModelListPropertyChangeFilter propertyObserverFilter =
                new ModelListPropertyChangeFilter(
                        this::onPropertyChange, modelList, CollectionUtil.newHashSet(PROPERTY_FOO));
        Assert.assertEquals(1, mCallbackCounter);

        propertyModel.set(PROPERTY_BAR, true);
        Assert.assertEquals(1, mCallbackCounter);

        propertyModel.set(PROPERTY_FOO, true);
        Assert.assertEquals(2, mCallbackCounter);

        propertyModel.set(PROPERTY_FOO, false);
        Assert.assertEquals(3, mCallbackCounter);

        propertyObserverFilter.destroy();

        propertyModel.set(PROPERTY_FOO, true);
        Assert.assertEquals(3, mCallbackCounter);
    }

    @Test
    public void testAddRemoveTriggers() {
        ModelList modelList = new ModelList();
        ModelListPropertyChangeFilter propertyObserverFilter =
                new ModelListPropertyChangeFilter(
                        this::onPropertyChange, modelList, CollectionUtil.newHashSet(PROPERTY_FOO));
        Assert.assertEquals(1, mCallbackCounter);

        PropertyModel propertyModel = new PropertyModel(PROPERTY_FOO, PROPERTY_BAR);
        modelList.add(new ListItem(/* type= */ 0, propertyModel));
        Assert.assertEquals(2, mCallbackCounter);

        modelList.removeAt(0);
        Assert.assertEquals(3, mCallbackCounter);

        modelList.add(new ListItem(/* type= */ 0, propertyModel));
        Assert.assertEquals(4, mCallbackCounter);

        propertyObserverFilter.destroy();

        modelList.removeAt(0);
        Assert.assertEquals(4, mCallbackCounter);
    }

    @Test
    public void testRemoveStopsObserving() {
        ModelList modelList = new ModelList();
        PropertyModel propertyModel = new PropertyModel(PROPERTY_FOO, PROPERTY_BAR);
        modelList.add(new ListItem(/* type= */ 0, propertyModel));

        ModelListPropertyChangeFilter propertyObserverFilter =
                new ModelListPropertyChangeFilter(
                        this::onPropertyChange, modelList, CollectionUtil.newHashSet(PROPERTY_FOO));
        Assert.assertEquals(1, mCallbackCounter);

        modelList.removeAt(0);
        Assert.assertEquals(2, mCallbackCounter);

        propertyModel.set(PROPERTY_FOO, true);
        Assert.assertEquals(2, mCallbackCounter);
    }
}
