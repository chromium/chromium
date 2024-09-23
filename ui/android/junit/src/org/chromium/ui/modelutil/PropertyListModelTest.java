// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;

import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests to validate correctness of the PropertyListModel. Mainly that sub-observers are added and
 * removed at the correct times.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PropertyListModelTest implements ListObservable.ListObserver<PropertyKey> {
    private static final int METHOD_COUNT = 36;
    private static final PropertyModel.WritableIntPropertyKey INTEGER_KEY =
            new PropertyModel.WritableIntPropertyKey();

    private PropertyListModel<PropertyModel, PropertyKey> mListModel;

    private int mOnRangeChangedCalled;
    private PropertyKey mPayload;
    private int mIndex;
    private int mCount;

    @Before
    public void setUp() {
        mListModel = new PropertyListModel<>();
        mListModel.addObserver(this);
        reset();
    }

    public void reset() {
        mOnRangeChangedCalled = 0;
    }

    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        mPayload = null;
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        mPayload = null;
    }

    @Override
    public void onItemRangeChanged(
            ListObservable<PropertyKey> source,
            int index,
            int count,
            @Nullable PropertyKey payload) {
        mIndex = index;
        mCount = count;
        mPayload = payload;
        mOnRangeChangedCalled += 1;
    }

    @Test
    public void addOneItem() {
        PropertyModel m = new PropertyModel.Builder(INTEGER_KEY).build();
        mListModel.add(m);

        // Verify that changes are propagated (observer was added to m).
        m.set(INTEGER_KEY, 10);
        assertThat(mOnRangeChangedCalled, equalTo(1));
        assertThat(mIndex, equalTo(0));
        assertThat(mCount, equalTo(1));
        assertThat(mPayload, equalTo(INTEGER_KEY));
    }

    @Test
    public void addAllItems() {
        List<PropertyModel> collection = new ArrayList<>();
        collection.add(new PropertyModel.Builder(INTEGER_KEY).build());
        collection.add(new PropertyModel.Builder(INTEGER_KEY).build());
        collection.add(new PropertyModel.Builder(INTEGER_KEY).build());

        mListModel.addAll(collection);

        // Verify that an observer was attached to each model.
        collection.get(0).set(INTEGER_KEY, 1);
        collection.get(1).set(INTEGER_KEY, 5);
        collection.get(2).set(INTEGER_KEY, 10);
        assertThat(mOnRangeChangedCalled, equalTo(3));

        // Index and parameters are that of the last call.
        assertThat(mIndex, equalTo(2));
        assertThat(mCount, equalTo(1));
        assertThat(mPayload, equalTo(INTEGER_KEY));
    }

    @Test
    public void addAllSimpleList() {
        ListModelBase<PropertyModel, Void> simpleList = new ListModelBase<>();
        simpleList.add(new PropertyModel.Builder(INTEGER_KEY).build());
        simpleList.add(new PropertyModel.Builder(INTEGER_KEY).build());
        simpleList.add(new PropertyModel.Builder(INTEGER_KEY).build());
        simpleList.add(new PropertyModel.Builder(INTEGER_KEY).build());

        mListModel.addAll(simpleList);

        // Verify that an observer was attached to each model.
        simpleList.get(0).set(INTEGER_KEY, 1);
        simpleList.get(1).set(INTEGER_KEY, 5);
        simpleList.get(2).set(INTEGER_KEY, 10);
        simpleList.get(3).set(INTEGER_KEY, 20);
        assertThat(mOnRangeChangedCalled, equalTo(4));

        // Index and parameters are that of the last call.
        assertThat(mIndex, equalTo(3));
        assertThat(mCount, equalTo(1));
        assertThat(mPayload, equalTo(INTEGER_KEY));
    }

    @Test
    public void addOneItemWithPosition() {
        PropertyModel m = new PropertyModel.Builder(INTEGER_KEY).build();
        PropertyModel n = new PropertyModel.Builder(INTEGER_KEY).build();
        mListModel.add(m);
        mListModel.add(0, n);
        reset();

        // Verify that an observer was attached to the model itself.
        m.set(INTEGER_KEY, 10);
        n.set(INTEGER_KEY, 5);
        assertThat(mOnRangeChangedCalled, equalTo(2));
        assertThat(mIndex, equalTo(0));
        assertThat(mCount, equalTo(1));
        assertThat(mPayload, equalTo(INTEGER_KEY));
    }

    @Test
    public void removeOneItem() {
        PropertyModel m = new PropertyModel.Builder(INTEGER_KEY).build();
        mListModel.add(m);
        mListModel.remove(m);
        reset();

        // Verify that the model observer has been removed.
        m.set(INTEGER_KEY, 5);
        assertThat(mOnRangeChangedCalled, equalTo(0));
    }

    @Test
    public void removeOneItemWithPosition() {
        PropertyModel m = new PropertyModel.Builder(INTEGER_KEY).build();
        mListModel.add(m);
        mListModel.removeAt(0);
        reset();

        // Verify that the model observer has been removed.
        m.set(INTEGER_KEY, 5);
        assertThat(mOnRangeChangedCalled, equalTo(0));
    }

    @Test
    public void removeRange() {
        PropertyModel m = new PropertyModel.Builder(INTEGER_KEY).build();
        mListModel.add(m);
        mListModel.removeRange(0, 1);
        reset();

        // Verify that the model observer has been removed.
        m.set(INTEGER_KEY, 5);
        assertThat(mOnRangeChangedCalled, equalTo(0));
    }

    @Test
    public void update() {
        PropertyModel m = new PropertyModel.Builder(INTEGER_KEY).build();
        PropertyModel n = new PropertyModel.Builder(INTEGER_KEY).build();
        mListModel.add(m);
        mListModel.update(0, n);

        reset();

        // Verify that changes to old model are not captured.
        m.set(INTEGER_KEY, 10);
        assertThat(mOnRangeChangedCalled, equalTo(0));

        // Verify that changes to new model are captured.
        n.set(INTEGER_KEY, 5);
        assertThat(mOnRangeChangedCalled, equalTo(1));
        assertThat(mIndex, equalTo(0));
        assertThat(mCount, equalTo(1));
        assertThat(mPayload, equalTo(INTEGER_KEY));
    }

    @Test
    public void set() {
        PropertyModel m = new PropertyModel.Builder(INTEGER_KEY).build();
        PropertyModel n = new PropertyModel.Builder(INTEGER_KEY).build();
        List<PropertyModel> collection = new ArrayList<>();
        collection.add(n);

        mListModel.add(m);
        mListModel.set(collection);
        reset();

        // Verify that changes to old model are not captured.
        m.set(INTEGER_KEY, 10);
        assertThat(mOnRangeChangedCalled, equalTo(0));

        // Verify that changes to new model are captured.
        n.set(INTEGER_KEY, 5);
        assertThat(mOnRangeChangedCalled, equalTo(1));
        assertThat(mIndex, equalTo(0));
        assertThat(mCount, equalTo(1));
        assertThat(mPayload, equalTo(INTEGER_KEY));
    }

    @Test
    public void methodCount() {
        // Ensure that new methods within ListModelBase, etc will properly add/remove
        // observers in the PropertyListModel class.
        assertThat(PropertyListModel.class.getMethods().length, equalTo(METHOD_COUNT));
    }
}
