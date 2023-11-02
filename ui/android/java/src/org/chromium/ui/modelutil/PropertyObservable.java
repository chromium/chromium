// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;

import java.util.Collection;

/**
 * A base class for models maintaining a set of properties.
 *
 * @param <T> The type of the property key used for uniquely identifying properties.
 */
public abstract class PropertyObservable<T> {
    /**
     * An observer to be notified of changes to a {@link PropertyObservable}.
     *
     * @param <T> The type of the property key used for uniquely identifying properties.
     */
    public interface PropertyObserver<T> {
        /**
         * Notifies that the given {@code property} of the observed {@code source} has changed.
         * @param source The object whose property has changed
         * @param propertyKey The key of the property that has changed.
         */
        void onPropertyChanged(PropertyObservable<T> source, @Nullable T propertyKey);
    }

    private final ObserverList<PropertyObserver<T>> mObservers = new ObserverList<>();

    /**
     * @param observer An observer to be notified of changes to the model.
     */
    public void addObserver(PropertyObserver<T> observer) {
        mObservers.addObserver(observer);
    }

    /**
     * @param observer The observer to remove.
     */
    public void removeObserver(PropertyObserver<T> observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * @return A collection of all properties of this model that have been set. The returned
     *         collection should not be modified.
     */
    public abstract Collection<T> getAllSetProperties();

    /**
     * @return A collection of all properties of this model. The returned collection should not be
     *         modified.
     */
    public abstract Collection<T> getAllProperties();

    /**
     * Notifies observers that the property identified by {@code propertyKey} has changed.
     *
     * @param propertyKey The key of the property that has changed.
     */
    protected void notifyPropertyChanged(T propertyKey) {
        for (PropertyObserver<T> observer : mObservers) {
            observer.onPropertyChanged(this, propertyKey);
        }
    }
}
