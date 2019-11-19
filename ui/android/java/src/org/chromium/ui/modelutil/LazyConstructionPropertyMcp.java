// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import androidx.annotation.Nullable;

import org.chromium.ui.ViewProvider;

import java.util.HashSet;
import java.util.Set;

/**
 * ModelChangeProcessor for property-observable models that can be visible or hidden.
 * Lazily creates the view the first time the model is shown, and queues up property changes while
 * the model is hidden, dispatching them when it is shown.
 * @param <M> The model type
 * @param <V> The view type
 * @param <P> The property type for the model
 */
public class LazyConstructionPropertyMcp<M extends PropertyObservable<P>, V, P>
        implements PropertyObservable.PropertyObserver<P> {
    /**
     * Functional interface to determine whether the model is visible.
     * @param <T> The model type.
     */
    public interface VisibilityPredicate<T> { boolean isVisible(T item); }

    private final M mModel;
    private final P mVisibilityProperty;
    private final VisibilityPredicate<M> mVisibilityPredicate;
    private final ViewProvider<V> mViewProvider;
    private final PropertyModelChangeProcessor.ViewBinder<M, V, P> mViewBinder;

    private boolean mPendingViewCreation;
    private @Nullable V mView;
    private final Set<P> mPendingProperties = new HashSet<>();

    public LazyConstructionPropertyMcp(M model, P visibilityProperty,
            VisibilityPredicate<M> visibilityPredicate, ViewProvider<V> viewProvider,
            PropertyModelChangeProcessor.ViewBinder<M, V, P> viewBinder) {
        assert visibilityProperty != null;
        mModel = model;
        mVisibilityProperty = visibilityProperty;
        mVisibilityPredicate = visibilityPredicate;
        mViewProvider = viewProvider;
        mViewBinder = viewBinder;

        mPendingProperties.addAll(mModel.getAllSetProperties());

        mViewProvider.whenLoaded(this::onViewCreated);

        // The model should start out hidden.
        assert !mVisibilityPredicate.isVisible(mModel);

        // The visibility property should be set initially, to avoid spurious property change
        // notifications that would cause the view to be inflated prematurely.
        assert mPendingProperties.contains(mVisibilityProperty);

        mModel.addObserver(this);
    }

    public static <M extends PropertyModel, V> LazyConstructionPropertyMcp<M, V, PropertyKey>
    create(M model, PropertyModel.WritableBooleanPropertyKey visibilityProperty,
            ViewProvider<V> viewFactory,
            PropertyModelChangeProcessor.ViewBinder<M, V, PropertyKey> viewBinder) {
        return new LazyConstructionPropertyMcp<>(model, visibilityProperty,
                item -> item.get(visibilityProperty), viewFactory, viewBinder);
    }

    private void flushPendingUpdates() {
        boolean pendingVisibilityUpdate = false;
        for (P property : mPendingProperties) {
            if (property == mVisibilityProperty) {
                pendingVisibilityUpdate = true;
                continue;
            }
            mViewBinder.bind(mModel, mView, property);
        }
        // Defer sending the visibility update until all prior set properties are dispatched.
        if (pendingVisibilityUpdate) mViewBinder.bind(mModel, mView, mVisibilityProperty);
        mPendingProperties.clear();
    }

    @Override
    public void onPropertyChanged(PropertyObservable<P> source, @Nullable P propertyKey) {
        assert source == mModel;

        mPendingProperties.add(propertyKey);

        // If the model is hidden, don't flush property updates yet (unless the updated property is
        // visibility).
        if (!mVisibilityPredicate.isVisible(mModel) && propertyKey != mVisibilityProperty) {
            return;
        }

        if (mView == null) {
            // If the view is already being created, do nothing.
            if (mPendingViewCreation) return;

            mPendingViewCreation = true;
            mViewProvider.inflate();
            return;
        }

        flushPendingUpdates();
    }

    private void onViewCreated(V v) {
        mView = v;
        mPendingViewCreation = false;
        flushPendingUpdates();
    }
}
