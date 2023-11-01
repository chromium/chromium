// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.ListObservable.ListObserver;

import java.util.Arrays;

/** Basic test ensuring the {@link ListModelBase} notifies listeners properly. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SimpleListObservableTest {
    @Mock private ListObserver<Integer> mObserver;

    private ListModelBase<Integer, Integer> mIntegerList = new ListModelBase<>();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mIntegerList.addObserver(mObserver);
    }

    @After
    public void tearDown() {
        mIntegerList.removeObserver(mObserver);
    }

    @Test
    public void testNotifiesSuccessfulInsertions() {
        // Replacing an empty list with a non-empty one is always an insertion.
        assertThat(mIntegerList.size(), is(0));
        mIntegerList.set(new Integer[] {333, 88888888, 22});
        verify(mObserver).onItemRangeInserted(mIntegerList, 0, 3);
        assertThat(mIntegerList.size(), is(3));
        assertThat(mIntegerList.get(1), is(88888888));

        // Adding Items is always an insertion.
        mIntegerList.add(55555);
        verify(mObserver).onItemRangeInserted(mIntegerList, 3, 1);
        assertThat(mIntegerList.size(), is(4));
        assertThat(mIntegerList.get(3), is(55555));

        // Adding multiple items also triggers event.
        mIntegerList.addAll(Arrays.asList(333, 88888888, 22), 2);
        verify(mObserver).onItemRangeInserted(mIntegerList, 2, 3);
    }

    @Test
    public void testModelNotifiesSuccessfulRemoval() {
        Integer eightEights = 88888888;
        mIntegerList.set(new Integer[] {333, eightEights, 22});
        assertThat(mIntegerList.size(), is(3));

        // Removing any item by instance is always a removal.
        mIntegerList.remove(eightEights);
        verify(mObserver).onItemRangeRemoved(mIntegerList, 1, 1);

        // Setting an empty list is a removal of all items.
        mIntegerList.set(new Integer[] {});
        verify(mObserver).onItemRangeRemoved(mIntegerList, 0, 2);
    }

    @Test
    public void testModelNotifiesSuccessfulMove() {
        Integer eightEights = 88888888;
        mIntegerList.set(new Integer[] {333, eightEights, 22});
        assertThat(mIntegerList.size(), is(3));

        // Moving any item forward is a move.
        mIntegerList.move(1, 0);
        verify(mObserver).onItemMoved(mIntegerList, 1, 0);
        assertThat(mIntegerList.get(0), is(eightEights));
        assertThat(mIntegerList.get(1), is(333));
        assertThat(mIntegerList.get(2), is(22));
        mIntegerList.set(new Integer[] {333, eightEights, 22});

        // Moving any item backward is a move.
        mIntegerList.move(1, 2);
        verify(mObserver).onItemMoved(mIntegerList, 1, 2);
        assertThat(mIntegerList.get(0), is(333));
        assertThat(mIntegerList.get(1), is(22));
        assertThat(mIntegerList.get(2), is(eightEights));
    }

    @Test
    public void testModelNotifiesReplacedDataAndRemoval() {
        // The initial setting is an insertion.
        mIntegerList.set(new Integer[] {333, 88888888, 22});
        verify(mObserver).onItemRangeInserted(mIntegerList, 0, 3);

        // Setting a smaller number of items is a removal and a change.
        mIntegerList.set(new Integer[] {4444, 22});
        verify(mObserver).onItemRangeChanged(mIntegerList, 0, 2, null);
        verify(mObserver).onItemRangeRemoved(mIntegerList, 2, 1);
    }

    @Test
    public void testModelNotifiesReplacedDataAndInsertion() {
        // The initial setting is an insertion.
        mIntegerList.set(new Integer[] {1234, 56});
        verify(mObserver).onItemRangeInserted(mIntegerList, 0, 2);

        // Setting a larger number of items is an insertion and a change.
        mIntegerList.set(new Integer[] {4444, 22, 1, 666666});
        verify(mObserver).onItemRangeChanged(mIntegerList, 0, 2, null);
        verify(mObserver).onItemRangeInserted(mIntegerList, 2, 2);

        // Setting empty data is a removal.
        mIntegerList.set(new Integer[] {});
        verify(mObserver).onItemRangeRemoved(mIntegerList, 0, 4);

        // Replacing an empty list with another empty list is a no-op.
        mIntegerList.set(new Integer[] {});
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    public void testAddAllSimpleList() {
        // Initialize the lists.
        mIntegerList.set(new Integer[] {1, 2, 3});
        ListModelBase<Integer, Integer> list = new ListModelBase<>();

        // Test adding to the back.
        list.set(new Integer[] {4, 5});
        mIntegerList.addAll(list);
        verify(mObserver).onItemRangeInserted(mIntegerList, 3, 2);
        assertEquals("Wrong list size after insertion.", 5, mIntegerList.size());
        assertThat("Wrong value found at index.", mIntegerList.get(3), is(4));
        assertThat("Wrong value found at index.", mIntegerList.get(4), is(5));

        // Test adding to somewhere in the middle.
        list.set(new Integer[] {6, 7});
        mIntegerList.addAll(list, 2);
        verify(mObserver).onItemRangeInserted(mIntegerList, 2, 2);
        assertEquals("Wrong list size after insertion.", 7, mIntegerList.size());
        assertThat("Wrong value found at index.", mIntegerList.get(2), is(6));
        assertThat("Wrong value found at index.", mIntegerList.get(3), is(7));
        assertThat("Wrong value found at index.", mIntegerList.get(4), is(3));
    }
}
