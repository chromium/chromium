// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.modelutil;

import androidx.annotation.Nullable;

/**
 * An interface for models notifying about changes to a list of items. Note that ListObservable
 * models do not need to be implemented as a list. Internally they may use any structure to organize
 * their items. Note also that this class on purpose does not expose an item type (it only exposes a
 * <i>payload</i> type for partial change notifications), nor does it give access to the list
 * contents. This is because the list might not be homogeneous -- it could represent items of vastly
 * different types that don't share a common base class. Use the {@link ListModel}
 * subclass for homogeneous lists.
 * @param <P> The parameter type for the payload for partial updates. Use {@link Void} for
 *         implementations that don't support partial updates.
 */
public interface ListObservable<P> {
    /**
     * @param observer An observer to be notified of changes to the model.
     */
    void addObserver(ListObserver<P> observer);

    /** @param observer The observer to remove. */
    void removeObserver(ListObserver<P> observer);

    /**
     * An observer to be notified of changes to a {@link ListObservable}.
     * @param <P> The parameter type for the payload for partial updates. Use {@link Void} for
     *         implementations that don't support partial updates.
     */
    interface ListObserver<P> {
        /**
         * Notifies that {@code count} items starting at position {@code index} under the
         * {@code source} have been added.
         *
         * @param source The list to which items have been added.
         * @param index The starting position of the range of added items.
         * @param count The number of added items.
         */
        default void onItemRangeInserted(ListObservable source, int index, int count) {}

        /**
         * Notifies that {@code count} items starting at position {@code index} under the
         * {@code source} have been removed.
         *
         * @param source The list from which items have been removed.
         * @param index The starting position of the range of removed items.
         * @param count The number of removed items.
         */
        default void onItemRangeRemoved(ListObservable source, int index, int count) {}

        /**
         * Notifies that {@code count} items starting at position {@code index} under the
         * {@code source} have changed, with an optional payload object.
         *
         * @param source The list whose items have changed.
         * @param index The starting position of the range of changed items.
         * @param count The number of changed items.
         * @param payload Optional parameter, use {@code null} to identify a "full" update.
         */
        default void onItemRangeChanged(
                ListObservable<P> source, int index, int count, @Nullable P payload) {}

        /**
         * Notifies that item at position {@code curIndex} will be moved to {@code newIndex}.
         *
         * @param curIndex Current position of the moved item.
         * @param newIndex New position of the moved item.
         */
        default void onItemMoved(ListObservable source, int curIndex, int newIndex) {}
    }
}
