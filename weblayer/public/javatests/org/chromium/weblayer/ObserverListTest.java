// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

import java.util.Collection;
import java.util.Iterator;
import java.util.NoSuchElementException;

/**
 * Tests for (@link ObserverList}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ObserverListTest {
    interface Observer {
        void observe(int x);
    }

    private static class Foo implements Observer {
        private final int mScalar;
        private int mTotal;

        Foo(int scalar) {
            mScalar = scalar;
        }

        @Override
        public void observe(int x) {
            mTotal += x * mScalar;
        }
    }

    /**
     * An observer which add a given Observer object to the list when observe is called.
     */
    private static class FooAdder implements Observer {
        private final ObserverList<Observer> mList;
        private final Observer mLucky;

        FooAdder(ObserverList<Observer> list, Observer oblivious) {
            mList = list;
            mLucky = oblivious;
        }

        @Override
        public void observe(int x) {
            mList.addObserver(mLucky);
        }
    }

    /**
     * An observer which removes a given Observer object from the list when observe is called.
     */
    private static class FooRemover implements Observer {
        private final ObserverList<Observer> mList;
        private final Observer mDoomed;

        FooRemover(ObserverList<Observer> list, Observer innocent) {
            mList = list;
            mDoomed = innocent;
        }

        @Override
        public void observe(int x) {
            mList.removeObserver(mDoomed);
        }
    }

    private static <T> int getSizeOfIterable(Iterable<T> iterable) {
        if (iterable instanceof Collection<?>) return ((Collection<?>) iterable).size();
        int num = 0;
        for (T el : iterable) num++;
        return num;
    }

    @Test
    @SmallTest
    public void testRemoveWhileIteration() {
        ObserverList<Observer> observerList = new ObserverList<Observer>();
        Foo a = new Foo(1);
        Foo b = new Foo(-1);
        Foo c = new Foo(1);
        Foo d = new Foo(-1);
        Foo e = new Foo(-1);
        FooRemover evil = new FooRemover(observerList, c);

        observerList.addObserver(a);
        observerList.addObserver(b);

        for (Observer obs : observerList) obs.observe(10);

        // Removing an observer not in the list should do nothing.
        observerList.removeObserver(e);

        observerList.addObserver(evil);
        observerList.addObserver(c);
        observerList.addObserver(d);

        for (Observer obs : observerList) obs.observe(10);

        // observe should be called twice on a.
        Assert.assertEquals(20, a.mTotal);
        // observe should be called twice on b.
        Assert.assertEquals(-20, b.mTotal);
        // evil removed c from the observerList before it got any callbacks.
        Assert.assertEquals(0, c.mTotal);
        // observe should be called once on d.
        Assert.assertEquals(-10, d.mTotal);
        // e was never added to the list, observe should not be called.
        Assert.assertEquals(0, e.mTotal);
    }

    @Test
    @SmallTest
    public void testAddWhileIteration() {
        ObserverList<Observer> observerList = new ObserverList<Observer>();
        Foo a = new Foo(1);
        Foo b = new Foo(-1);
        Foo c = new Foo(1);
        FooAdder evil = new FooAdder(observerList, c);

        observerList.addObserver(evil);
        observerList.addObserver(a);
        observerList.addObserver(b);

        for (Observer obs : observerList) obs.observe(10);

        Assert.assertTrue(observerList.hasObserver(c));
        Assert.assertEquals(10, a.mTotal);
        Assert.assertEquals(-10, b.mTotal);
        Assert.assertEquals(0, c.mTotal);
    }

    @Test
    @SmallTest
    public void testIterator() {
        ObserverList<Integer> observerList = new ObserverList<Integer>();
        observerList.addObserver(5);
        observerList.addObserver(10);
        observerList.addObserver(15);
        Assert.assertEquals(3, getSizeOfIterable(observerList));

        observerList.removeObserver(10);
        Assert.assertEquals(2, getSizeOfIterable(observerList));

        Iterator<Integer> it = observerList.iterator();
        Assert.assertTrue(it.hasNext());
        Assert.assertTrue(5 == it.next());
        Assert.assertTrue(it.hasNext());
        Assert.assertTrue(15 == it.next());
        Assert.assertFalse(it.hasNext());

        boolean removeExceptionThrown = false;
        try {
            it.remove();
            Assert.fail("Expecting UnsupportedOperationException to be thrown here.");
        } catch (UnsupportedOperationException e) {
            removeExceptionThrown = true;
        }
        Assert.assertTrue(removeExceptionThrown);
        Assert.assertEquals(2, getSizeOfIterable(observerList));

        boolean noElementExceptionThrown = false;
        try {
            it.next();
            Assert.fail("Expecting NoSuchElementException to be thrown here.");
        } catch (NoSuchElementException e) {
            noElementExceptionThrown = true;
        }
        Assert.assertTrue(noElementExceptionThrown);
    }

    @Test
    @SmallTest
    public void testRewindableIterator() {
        ObserverList<Integer> observerList = new ObserverList<Integer>();
        observerList.addObserver(5);
        observerList.addObserver(10);
        observerList.addObserver(15);
        Assert.assertEquals(3, getSizeOfIterable(observerList));

        ObserverList.RewindableIterator<Integer> it = observerList.rewindableIterator();
        Assert.assertTrue(it.hasNext());
        Assert.assertTrue(5 == it.next());
        Assert.assertTrue(it.hasNext());
        Assert.assertTrue(10 == it.next());
        Assert.assertTrue(it.hasNext());
        Assert.assertTrue(15 == it.next());
        Assert.assertFalse(it.hasNext());

        it.rewind();

        Assert.assertTrue(it.hasNext());
        Assert.assertTrue(5 == it.next());
        Assert.assertTrue(it.hasNext());
        Assert.assertTrue(10 == it.next());
        Assert.assertTrue(it.hasNext());
        Assert.assertTrue(15 == it.next());
        Assert.assertEquals(5, (int) observerList.mObservers.get(0));
        observerList.removeObserver(5);
        Assert.assertEquals(null, observerList.mObservers.get(0));

        it.rewind();

        Assert.assertEquals(10, (int) observerList.mObservers.get(0));
        Assert.assertTrue(it.hasNext());
        Assert.assertTrue(10 == it.next());
        Assert.assertTrue(it.hasNext());
        Assert.assertTrue(15 == it.next());
    }

    @Test
    @SmallTest
    public void testAddObserverReturnValue() {
        ObserverList<Object> observerList = new ObserverList<Object>();

        Object a = new Object();
        Assert.assertTrue(observerList.addObserver(a));
        Assert.assertFalse(observerList.addObserver(a));

        Object b = new Object();
        Assert.assertTrue(observerList.addObserver(b));
        Assert.assertFalse(observerList.addObserver(null));
    }

    @Test
    @SmallTest
    public void testRemoveObserverReturnValue() {
        ObserverList<Object> observerList = new ObserverList<Object>();

        Object a = new Object();
        Object b = new Object();
        observerList.addObserver(a);
        observerList.addObserver(b);

        Assert.assertTrue(observerList.removeObserver(a));
        Assert.assertFalse(observerList.removeObserver(a));
        Assert.assertFalse(observerList.removeObserver(new Object()));
        Assert.assertTrue(observerList.removeObserver(b));
        Assert.assertFalse(observerList.removeObserver(null));

        // If we remove an object while iterating, it will be replaced by 'null'.
        observerList.addObserver(a);
        Assert.assertTrue(observerList.removeObserver(a));
        Assert.assertFalse(observerList.removeObserver(null));
    }

    @Test
    @SmallTest
    public void testSize() {
        ObserverList<Object> observerList = new ObserverList<Object>();

        Assert.assertEquals(0, observerList.size());
        Assert.assertTrue(observerList.isEmpty());

        observerList.addObserver(null);
        Assert.assertEquals(0, observerList.size());
        Assert.assertTrue(observerList.isEmpty());

        Object a = new Object();
        observerList.addObserver(a);
        Assert.assertEquals(1, observerList.size());
        Assert.assertFalse(observerList.isEmpty());

        observerList.addObserver(a);
        Assert.assertEquals(1, observerList.size());
        Assert.assertFalse(observerList.isEmpty());

        observerList.addObserver(null);
        Assert.assertEquals(1, observerList.size());
        Assert.assertFalse(observerList.isEmpty());

        Object b = new Object();
        observerList.addObserver(b);
        Assert.assertEquals(2, observerList.size());
        Assert.assertFalse(observerList.isEmpty());

        observerList.removeObserver(null);
        Assert.assertEquals(2, observerList.size());
        Assert.assertFalse(observerList.isEmpty());

        observerList.removeObserver(new Object());
        Assert.assertEquals(2, observerList.size());
        Assert.assertFalse(observerList.isEmpty());

        observerList.removeObserver(b);
        Assert.assertEquals(1, observerList.size());
        Assert.assertFalse(observerList.isEmpty());

        observerList.removeObserver(b);
        Assert.assertEquals(1, observerList.size());
        Assert.assertFalse(observerList.isEmpty());

        observerList.removeObserver(a);
        Assert.assertEquals(0, observerList.size());
        Assert.assertTrue(observerList.isEmpty());

        observerList.removeObserver(a);
        observerList.removeObserver(b);
        observerList.removeObserver(null);
        observerList.removeObserver(new Object());
        Assert.assertEquals(0, observerList.size());
        Assert.assertTrue(observerList.isEmpty());

        observerList.addObserver(new Object());
        observerList.addObserver(new Object());
        observerList.addObserver(new Object());
        observerList.addObserver(a);
        Assert.assertEquals(4, observerList.size());
        Assert.assertFalse(observerList.isEmpty());

        observerList.clear();
        Assert.assertEquals(0, observerList.size());
        Assert.assertTrue(observerList.isEmpty());

        observerList.removeObserver(a);
        observerList.removeObserver(b);
        observerList.removeObserver(null);
        observerList.removeObserver(new Object());
        Assert.assertEquals(0, observerList.size());
        Assert.assertTrue(observerList.isEmpty());
    }
}
