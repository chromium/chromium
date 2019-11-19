// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import androidx.annotation.NonNull;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;

/**
 * Base class for a {@link ListObservable} containing a {@link SimpleList} of items that support
 * sending partial change notifications. If the list item type does not support partial change
 * notifications, use the {@link ListModel} subclass.
 * It allows models to compose different ListObservables.
 * @param <T> The object type that this class manages in a list.
 * @param <P> The payload type for partial change notifications.
 */
public class ListModelBase<T, P> extends ListObservableImpl<P> implements SimpleList<T> {
    private final List<T> mItems = new ArrayList<>();

    /**
     * Returns the item at the given position.
     * @param index The position to get the item from.
     * @return Returns the found item.
     */
    @Override
    public T get(int index) {
        return mItems.get(index);
    }

    @Override
    public int size() {
        return mItems.size();
    }

    @NonNull
    @Override
    public Iterator<T> iterator() {
        return mItems.iterator();
    }

    /**
     * Appends a given {@code item} to the last position of the held {@link List}.
     * Notifies observers about the inserted item.
     * @param item The item to be stored.
     */
    public void add(T item) {
        mItems.add(item);
        notifyItemInserted(mItems.size() - 1);
    }

    /**
     * Inserts a given {@code item} at position {@code position} of the held {@link List}.
     * Notifies observers about the inserted item.
     * @param position The position of the item to be inserted.
     * @param item The item to be inserted.
     */
    public void add(int position, T item) {
        mItems.add(position, item);
        notifyItemInserted(position);
    }

    /**
     * Appends all given {@code items} to the last position of the held {@link List}.
     * Notifies observers about the inserted items.
     * @param items The items to be stored.
     */
    public void addAll(Collection<T> items) {
        int insertionIndex = mItems.size();
        mItems.addAll(items);
        notifyItemRangeInserted(insertionIndex, items.size());
    }

    /**
     * Removes a given item from the held {@link List}. Notifies observers about the removal.
     * @param item The item to be removed.
     */
    public void remove(T item) {
        int position = mItems.indexOf(item);
        removeAt(position);
    }

    /**
     * Removes an item by position from the held {@link List}. Notifies observers about the removal.
     * @param position The position of the item to be removed.
     * @return The item that has been removed.
     */
    public T removeAt(int position) {
        T item = mItems.remove(position);
        notifyItemRemoved(position);
        return item;
    }

    /**
     * Removes a range of {@code count} consecutive items from the held {@link List}, starting at
     * {@code startPosition}. Notifies observers about the removal.
     * @param startPosition The start position of the range of items to be removed.
     * @param count The number of items to be removed.
     */
    public void removeRange(int startPosition, int count) {
        mItems.subList(startPosition, startPosition + count).clear();
        notifyItemRangeRemoved(startPosition, count);
    }

    /**
     * Convenience method to replace all held items with the given array of items.
     * @param newItems The array of items that should replace all held items.
     * @see #set(Collection)
     */
    public void set(T[] newItems) {
        set(Arrays.asList(newItems));
    }

    /**
     * Replaces all held items with the given collection of items, notifying observers about the
     * resulting insertions, deletions, changes, or combinations thereof.
     * @param newItems The collection of items that should replace all held items.
     */
    public void set(Collection<T> newItems) {
        int oldSize = mItems.size();
        int newSize = newItems.size();

        mItems.clear();
        mItems.addAll(newItems);

        int min = Math.min(oldSize, newSize);
        if (min > 0) notifyItemRangeChanged(0, min);

        if (newSize > oldSize) {
            notifyItemRangeInserted(min, newSize - oldSize);
        } else if (newSize < oldSize) {
            notifyItemRangeRemoved(min, oldSize - newSize);
        }
    }

    /**
     * Replaces a single {@code item} at the given {@code index}.
     * @param index The index of the item to be replaced.
     * @param item The item to be replaced.
     */
    public void update(int index, T item) {
        mItems.set(index, item);
        notifyItemRangeChanged(index, 1);
    }

    /**
     * @return The position of the given {@code item} in the held {@link List}.
     */
    public int indexOf(Object item) {
        return mItems.indexOf(item);
    }

    /**
     * Moves a single {@code item} from current {@code index} to new {@code index}.
     * @param curIndex The position of the item before move.
     * @param newIndex The position of the item after move.
     */
    public void move(int curIndex, int newIndex) {
        T item = mItems.remove(curIndex);
        if (newIndex == mItems.size()) {
            mItems.add(item);
        } else {
            mItems.add(newIndex, item);
        }
        notifyItemMoved(curIndex, newIndex);
    }

    /** Clear all items from the list. */
    public void clear() {
        if (size() > 0) removeRange(0, size());
    }
}
