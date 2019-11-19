// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

/**
 * A model change processor for use with a {@link PropertyObservable} model. The
 * {@link PropertyModelChangeProcessor} should be registered as a property observer of the model.
 * Internally uses a view binder to bind model properties to the toolbar view.
 * @param <M> The {@link PropertyObservable} model.
 * @param <V> The view object that is changing.
 * @param <P> The property of the view that changed.
 */
public class PropertyModelChangeProcessor<M extends PropertyObservable<P>, V, P> {
    /**
     * A generic view binder that associates a view with a model.
     * @param <M> The {@link PropertyObservable} model.
     * @param <V> The view object that is changing.
     * @param <P> The property of the view that changed.
     */
    public interface ViewBinder<M, V, P> { void bind(M model, V view, P propertyKey); }

    private final V mView;
    private final M mModel;
    private final ViewBinder<M, V, P> mViewBinder;

    private final PropertyObserver<P> mPropertyObserver = this::onPropertyChanged;

    /**
     * Construct a new PropertyModelChangeProcessor.
     * @param model The model containing the data to be bound.
     * @param view The view to which data will be bound.
     * @param viewBinder A class that binds the model to the view.
     * @param performInitialBind Whether all set model properties should be immediately bound.
     */
    private PropertyModelChangeProcessor(
            M model, V view, ViewBinder<M, V, P> viewBinder, boolean performInitialBind) {
        mModel = model;
        mView = view;
        mViewBinder = viewBinder;

        if (performInitialBind) {
            for (P property : model.getAllSetProperties()) {
                onPropertyChanged(model, property);
            }
        }

        model.addObserver(mPropertyObserver);
    }

    /**
     * Creates a new PropertyModelChangeProcessor observing the given {@code model}. All set model
     * properties will be bound.
     * @param model The model containing the data to be bound.
     * @param view The view to which data will be bound.
     * @param viewBinder A class that binds the model to the view.
     */
    public static <M extends PropertyObservable<P>, V, P> PropertyModelChangeProcessor<M, V, P>
    create(M model, V view, ViewBinder<M, V, P> viewBinder) {
        return create(model, view, viewBinder, true);
    }

    /**
     * Creates a new PropertyModelChangeProcessor observing the given {@code model}.
     * @param model The model containing the data to be bound.
     * @param view The view to which data will be bound.
     * @param viewBinder A class that binds the model to the view.
     * @param performInitialBind Whether all set model properties should be immediately bound.
     */
    public static <M extends PropertyObservable<P>, V, P> PropertyModelChangeProcessor<M, V, P>
    create(M model, V view, ViewBinder<M, V, P> viewBinder, boolean performInitialBind) {
        return new PropertyModelChangeProcessor<>(model, view, viewBinder, performInitialBind);
    }

    /**
     * To be called when the model should no longer be observed.
     */
    public void destroy() {
        mModel.removeObserver(mPropertyObserver);
    }

    private void onPropertyChanged(PropertyObservable<P> source, P propertyKey) {
        assert source == mModel;

        // TODO(bauerb): Add support for batching updates.
        mViewBinder.bind(mModel, mView, propertyKey);
    }
}