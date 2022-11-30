// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import androidx.annotation.Nullable;

import java.util.Collection;

/**
 * Represents a list of (property-)observable items, and notifies about changes to any of its items.
 *
 * @param <T> The type of item in the list.
 * @param <P> The property key type for {@code T} to be used as payload for partial updates.
 */
public class PropertyListModel<T extends PropertyObservable<P>, P> extends ListModelBase<T, P> {
    private final PropertyObservable.PropertyObserver<P> mPropertyObserver =
            this::onPropertyChanged;

    @Override
    public void add(T item) {
        super.add(item);
        item.addObserver(mPropertyObserver);
    }

    @Override
    public void add(int position, T item) {
        super.add(position, item);
        item.addObserver(mPropertyObserver);
    }

    @Override
    public void addAll(Collection<? extends T> items, int position) {
        super.addAll(items, position);
        for (T item : items) {
            item.addObserver(mPropertyObserver);
        }
    }

    @Override
    public void addAll(SimpleList<T> items, int insertionIndex) {
        super.addAll(items, insertionIndex);
        for (T item : items) {
            item.addObserver(mPropertyObserver);
        }
    }

    @Override
    public T removeAt(int position) {
        T item = super.removeAt(position);
        item.removeObserver(mPropertyObserver);
        return item;
    }

    @Override
    public void removeRange(int startPosition, int count) {
        for (int i = 0; i < count; i++) {
            get(startPosition + i).removeObserver(mPropertyObserver);
        }
        super.removeRange(startPosition, count);
    }

    @Override
    public void update(int index, T item) {
        get(index).removeObserver(mPropertyObserver);
        super.update(index, item);
        item.addObserver(mPropertyObserver);
    }

    @Override
    public void set(Collection<T> newItems) {
        for (T item : this) {
            item.removeObserver(mPropertyObserver);
        }
        super.set(newItems);
        for (T item : newItems) {
            item.addObserver(mPropertyObserver);
        }
    }

    private void onPropertyChanged(PropertyObservable<P> source, @Nullable P propertyKey) {
        notifyItemChanged(indexOf(source), propertyKey);
    }
}
