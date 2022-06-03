// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the ApplicationViewportInsetSupplier. */
@RunWith(BaseRobolectricTestRunner.class)
public class ApplicationViewportInsetSupplierTest {
    /** A callback with the ability to get the last value pushed to it. */
    private static class CapturingCallback<T> implements Callback<T> {
        private T mValue;

        @Override
        public void onResult(T result) {
            mValue = result;
        }

        public T getCapturedValue() {
            return mValue;
        }
    }

    private ApplicationViewportInsetSupplier mWindowApplicationInsetSupplier;
    private ObservableSupplierImpl<Integer> mFeatureInsetSupplier;
    private CapturingCallback<Integer> mWindowInsetObserver;

    @Before
    public void setUp() {
        mWindowApplicationInsetSupplier = new ApplicationViewportInsetSupplier();
        mFeatureInsetSupplier = new ObservableSupplierImpl<>();
        mWindowInsetObserver = new CapturingCallback<>();

        mWindowApplicationInsetSupplier.addSupplier(mFeatureInsetSupplier);
        mWindowApplicationInsetSupplier.addObserver(mWindowInsetObserver);
    }

    @Test
    public void testSupplierDidNotSetValue() {
        assertEquals("Observed value from supplier is incorrect.", (Integer) 0,
                mWindowApplicationInsetSupplier.get());
    }

    @Test
    public void testSupplierTriggersObserver() {
        mFeatureInsetSupplier.set(5);
        assertEquals("Observed value from supplier is incorrect.", (Integer) 5,
                mWindowInsetObserver.getCapturedValue());
    }

    @Test
    public void testSupplierTriggersObserver_multipleSuppliers_2() {
        ObservableSupplierImpl<Integer> secondSupplier = new ObservableSupplierImpl<>();
        mWindowApplicationInsetSupplier.addSupplier(secondSupplier);

        mFeatureInsetSupplier.set(5);
        secondSupplier.set(10);

        assertEquals("Observed value should be the max of the two supplied.", (Integer) 10,
                mWindowInsetObserver.getCapturedValue());
    }

    @Test
    public void testSupplierTriggersObserver_multipleSuppliers_3() {
        ObservableSupplierImpl<Integer> secondSupplier = new ObservableSupplierImpl<>();
        mWindowApplicationInsetSupplier.addSupplier(secondSupplier);

        ObservableSupplierImpl<Integer> thirdSupplier = new ObservableSupplierImpl<>();
        mWindowApplicationInsetSupplier.addSupplier(thirdSupplier);

        mFeatureInsetSupplier.set(5);
        secondSupplier.set(20);
        thirdSupplier.set(10);

        assertEquals("Observed value should be the max of the three supplied.", (Integer) 20,
                mWindowInsetObserver.getCapturedValue());
    }

    @Test
    public void testSupplierTriggersObserver_setBeforeAdded() {
        ObservableSupplierImpl<Integer> supplier = new ObservableSupplierImpl<>();
        supplier.set(20);

        mWindowApplicationInsetSupplier.addSupplier(supplier);

        assertEquals("The observer should have been triggered after the supplier was added.",
                (Integer) 20, mWindowInsetObserver.getCapturedValue());
    }

    @Test
    public void testSupplierRemoveTriggersEvent() {
        ObservableSupplierImpl<Integer> secondSupplier = new ObservableSupplierImpl<>();
        mWindowApplicationInsetSupplier.addSupplier(secondSupplier);

        ObservableSupplierImpl<Integer> thirdSupplier = new ObservableSupplierImpl<>();
        mWindowApplicationInsetSupplier.addSupplier(thirdSupplier);

        mFeatureInsetSupplier.set(5);
        secondSupplier.set(20);
        thirdSupplier.set(10);

        assertEquals("Observed value should be the max of the three supplied.", (Integer) 20,
                mWindowInsetObserver.getCapturedValue());

        mWindowApplicationInsetSupplier.removeSupplier(secondSupplier);

        assertEquals("Observed value should be the max of the two remaining.", (Integer) 10,
                mWindowInsetObserver.getCapturedValue());
    }

    @Test
    public void testAllSuppliersRemoved() {
        mFeatureInsetSupplier.set(5);

        assertEquals("Observed value from supplier is incorrect.", (Integer) 5,
                mWindowInsetObserver.getCapturedValue());

        mWindowApplicationInsetSupplier.removeSupplier(mFeatureInsetSupplier);

        assertEquals("Observed value should be 0 with no suppliers attached.", (Integer) 0,
                mWindowInsetObserver.getCapturedValue());
    }
}
