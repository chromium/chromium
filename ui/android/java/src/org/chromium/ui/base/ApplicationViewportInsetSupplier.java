// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

import java.util.HashSet;
import java.util.Set;

/**
 * A class responsible for managing multiple users of window viewport insets. The class that
 * actually updates the viewport insets listens to this class via {@link #addObserver(Callback)}.
 * Other features can also listen to this class to know if something is obscuring part of the
 * screen. Features that wish to update the viewport's insets can attach an inset supplier via
 * {@link #addSupplier(ObservableSupplier)}. This class will use the largest inset value to
 * adjust the viewport inset.
 *
 * In general:
 *  - Features that want to modify the inset should pass around the
 *    {@link ApplicationViewportInsetSupplier} object.
 *  - Features only interested in what the current inset is should pass around an
 *    {@link ObservableSupplier<Integer>} object.
 */
public class ApplicationViewportInsetSupplier extends ObservableSupplierImpl<Integer> {
    /** The list of inset providers that this class manages. */
    private final Set<ObservableSupplier<Integer>> mInsetSuppliers = new HashSet<>();

    /** The observer that gets attached to all inset providers. */
    private final Callback<Integer> mInsetSupplierObserver = (inset) -> computeInset();

    /** Default constructor. */
    ApplicationViewportInsetSupplier() {
        super();
        // Make sure this is initialized to 0 since "Integer" is an object and would be null
        // otherwise.
        super.set(0);
    }

    @VisibleForTesting
    public static ApplicationViewportInsetSupplier createForTests() {
        return new ApplicationViewportInsetSupplier();
    }

    /** Clean up observers and suppliers. */
    public void destroy() {
        for (ObservableSupplier<Integer> os : mInsetSuppliers) {
            os.removeObserver(mInsetSupplierObserver);
        }
        mInsetSuppliers.clear();
    }

    /** Compute the new inset based on the current registered providers. */
    private void computeInset() {
        int currentInset = 0;
        for (ObservableSupplier<Integer> os : mInsetSuppliers) {
            // Similarly to the constructor, check that the Integer object isn't null as the
            // supplier may not yet have supplied the initial value.
            currentInset = Math.max(currentInset, os.get() == null ? 0 : os.get());
        }

        super.set(currentInset);
    }

    @Override
    public void set(Integer value) {
        throw new IllegalStateException(
                "#set(...) should not be called directly on ApplicationViewportInsetSupplier.");
    }

    /** @param insetSupplier A supplier of bottom insets to be added. */
    public void addSupplier(ObservableSupplier<Integer> insetSupplier) {
        mInsetSuppliers.add(insetSupplier);
        insetSupplier.addObserver(mInsetSupplierObserver);
    }

    /** @param insetSupplier A supplier of bottom insets to be removed. */
    public void removeSupplier(ObservableSupplier<Integer> insetSupplier) {
        mInsetSuppliers.remove(insetSupplier);
        insetSupplier.removeObserver(mInsetSupplierObserver);
        computeInset();
    }
}
