// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

import java.util.HashSet;
import java.util.Set;

/**
 * A class responsible for managing multiple users of UI insets affecting
 * the visual viewport.
 *
 * Insetting the visual viewport overlays web content without resizing its
 * container (meaning it doesn't affect page layout); however, the user can
 * always scroll within the visual viewport to reveal overlaid content and
 * authors can respond to changes in the visual viewport.
 *
 * Features needing to know if something is obscuring part of the screen listen
 * to this class via {@link #addObserver(Callback)}. UI that wishes to inset
 * the visual viewport can attach an inset supplier via {@link
 * #addSupplier(ObservableSupplier)}.
 *
 * This class supports two kinds of inset suppliers: overlapping and stacking.
 *
 * Stacking suppliers are assumed to be presented "stacked", one of top (in the
 * y-axis) of the other. For example, the autofill keyboard accessory and the
 * on-screen keyboard are presented with the accessory appearing directly above
 * the keyboard. In this case, the keyboard inset is added to the accessory
 * inset to compute the total "stacking inset".
 *
 * Overlapping suppliers assume each supplier is attached to the viewport
 * bottom and don't take each other into account. For example, if a bottom info
 * bar is showing but a contextual search panel slides in from below, obscuring
 * the info bar. In this case, both the info bar and search panel provide their
 * own overlapping inset. The total "overlapping inset" is computed by taking
 * the largest value of all overlap suppliers.
 *
 * The final inset (the one provided to observers) is the largest between the
 * stacking and overlapping insets.
 *
 * In general:
 *  - Features that want to modify the inset should pass around the
 *    {@link ApplicationViewportInsetSupplier} object.
 *  - Features only interested in what the current inset is should pass around an
 *    {@link ObservableSupplier<Integer>} object.
 */
public class ApplicationViewportInsetSupplier
        extends ObservableSupplierImpl<Integer> implements Destroyable {
    /** The lists of inset providers that this class manages. */
    private final Set<ObservableSupplier<Integer>> mOverlappingInsetSuppliers = new HashSet<>();
    private final Set<ObservableSupplier<Integer>> mStackingInsetSuppliers = new HashSet<>();

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
    @Override
    public void destroy() {
        for (ObservableSupplier<Integer> os : mOverlappingInsetSuppliers) {
            os.removeObserver(mInsetSupplierObserver);
        }
        for (ObservableSupplier<Integer> os : mStackingInsetSuppliers) {
            os.removeObserver(mInsetSupplierObserver);
        }

        mOverlappingInsetSuppliers.clear();
        mStackingInsetSuppliers.clear();
    }

    /** Compute the new inset based on the current registered providers. */
    private void computeInset() {
        int stackingInset = 0;
        for (ObservableSupplier<Integer> os : mStackingInsetSuppliers) {
            // Similarly to the constructor, check that the Integer object isn't null as the
            // supplier may not yet have supplied the initial value.
            stackingInset += os.get() == null ? 0 : os.get();
        }

        int overlappingInset = 0;
        for (ObservableSupplier<Integer> os : mOverlappingInsetSuppliers) {
            // Similarly to the constructor, check that the Integer object isn't null as the
            // supplier may not yet have supplied the initial value.
            overlappingInset = Math.max(overlappingInset, os.get() == null ? 0 : os.get());
        }

        super.set(Math.max(stackingInset, overlappingInset));
    }

    @Override
    public void set(Integer value) {
        throw new IllegalStateException(
                "#set(...) should not be called directly on ApplicationViewportInsetSupplier.");
    }

    /**
     * Adds a supplier of viewport insets that overlap.
     *
     * Of all overlap insets, only the largest is applied to the final inset.
     *
     * @param insetSupplier A supplier of bottom insets to be added.
     */
    public void addOverlappingSupplier(ObservableSupplier<Integer> insetSupplier) {
        mOverlappingInsetSuppliers.add(insetSupplier);
        insetSupplier.addObserver(mInsetSupplierObserver);
    }

    /**
     * Adds a supplier of viewport insets that stack.
     *
     * Stacking insets are added together when applied to the final inset.
     *
     * @param insetSupplier A supplier of bottom insets to be added.
     */
    public void addStackingSupplier(ObservableSupplier<Integer> insetSupplier) {
        mStackingInsetSuppliers.add(insetSupplier);
        insetSupplier.addObserver(mInsetSupplierObserver);
    }

    /**
     * Removes a previously added supplier.
     *
     * The given supplier is removed regardless of whether it was overlapping or stacking.
     *
     * @param insetSupplier A supplier of bottom insets to be removed.
     */
    public void removeSupplier(ObservableSupplier<Integer> insetSupplier) {
        mOverlappingInsetSuppliers.remove(insetSupplier);
        mStackingInsetSuppliers.remove(insetSupplier);
        insetSupplier.removeObserver(mInsetSupplierObserver);
        computeInset();
    }
}
