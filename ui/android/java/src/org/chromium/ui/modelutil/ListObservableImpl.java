// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;

/**
 * Helper class for implementations of {@link ListObservable}, with some convenience methods for
 * sending notifications.
 * @param <P> The parameter type for the payload for partial updates. Use {@link Void} for
 *         implementations that don't support partial updates.
 */
public abstract class ListObservableImpl<P> implements ListObservable<P> {
    private final ObserverList<ListObserver<P>> mObservers = new ObserverList<>();

    @Override
    public void addObserver(ListObserver<P> observer) {
        boolean success = mObservers.addObserver(observer);
        assert success;
    }

    @Override
    public void removeObserver(ListObserver<P> observer) {
        boolean success = mObservers.removeObserver(observer);
        assert success;
    }

    protected final void notifyItemChanged(int index) {
        notifyItemRangeChanged(index, 1, null);
    }

    protected final void notifyItemRangeChanged(int index, int count) {
        notifyItemRangeChanged(index, count, null);
    }

    protected final void notifyItemChanged(int index, @Nullable P payload) {
        notifyItemRangeChanged(index, 1, payload);
    }

    protected final void notifyItemInserted(int index) {
        notifyItemRangeInserted(index, 1);
    }

    protected final void notifyItemRemoved(int index) {
        notifyItemRangeRemoved(index, 1);
    }

    /**
     * Notifies observers that {@code count} items starting at position {@code index} have been
     * added.
     *
     * @param index The starting position of the range of added items.
     * @param count The number of added items.
     */
    protected void notifyItemRangeInserted(int index, int count) {
        assert count > 0; // No spurious notifications
        for (ListObserver observer : mObservers) {
            observer.onItemRangeInserted(this, index, count);
        }
    }

    /**
     * Notifies observes that {@code count} items starting at position {@code index} have been
     * removed.
     *
     * @param index The starting position of the range of removed items.
     * @param count The number of removed items.
     */
    protected void notifyItemRangeRemoved(int index, int count) {
        assert count > 0; // No spurious notifications
        for (ListObserver observer : mObservers) {
            observer.onItemRangeRemoved(this, index, count);
        }
    }

    /**
     * Notifies observers that {@code count} items starting at position {@code index} under the
     * {@code source} have changed, with an optional payload object.
     *
     * @param index The starting position of the range of changed items.
     * @param count The number of changed items.
     * @param payload Optional parameter, use {@code null} to identify a "full" update.
     */
    protected void notifyItemRangeChanged(int index, int count, @Nullable P payload) {
        assert count > 0; // No spurious notifications
        for (ListObserver<P> observer : mObservers) {
            observer.onItemRangeChanged(this, index, count, payload);
        }
    }

    /**
     * Notifies observers that item at position {@code curIndex} will be moved to {@code newIndex}.
     *
     * @param curIndex Current position of the moved item.
     * @param newIndex New position of the moved item.
     */
    protected void notifyItemMoved(int curIndex, int newIndex) {
        for (ListObserver observer : mObservers) {
            observer.onItemMoved(this, curIndex, newIndex);
        }
    }
}
