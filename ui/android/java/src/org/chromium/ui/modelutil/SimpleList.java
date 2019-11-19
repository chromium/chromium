// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.modelutil;

import androidx.annotation.NonNull;

import java.util.AbstractList;
import java.util.Iterator;
import java.util.List;

/**
 * A minimal subset of the functionality of {@link List}, to allow easier implementation in
 * classes that already extend another class and therefore can't inherit from {@link AbstractList}.
 * @param <T> The type of list item.
 */
public interface SimpleList<T> extends Iterable<T> {
    /**
     * @return The size of the list.
     * @see List#size
     */
    int size();

    /**
     * Returns the item at the given position.
     * @param index The position to get the item from.
     * @return Returns the found item.
     * @see List#get
     */
    T get(int index);

    /**
     * @return An iterator over elements in the list. The iterator is not safe for concurrent
     * modifications and does not check whether the underlying list is being modified.
     */
    @Override
    @NonNull
    default Iterator<T> iterator() {
        return new Iterator<T>() {
            private int mI;

            @Override
            public boolean hasNext() {
                return mI < size();
            }

            @Override
            public T next() {
                return get(mI++);
            }
        };
    }
}
