// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.support.v7.widget.RecyclerView;

import androidx.annotation.Nullable;

/**
 * A model change processor for use with a {@link ListObservable} model. The
 * {@link ListModelChangeProcessor} should be registered as a list observer of the model.
 * Internally uses a view binder to bind model properties to a view like a TabLayout.
 *
 * Do not use this class to fill {@link RecyclerView}s - consider using the
 * {@link SimpleRecyclerViewMcp} which was specifically designed for that use case!
 *
 * @param <M> The {@link ListObservable} model.
 * @param <V> The view object that is changing.
 */
public class ListModelChangeProcessor<M extends ListObservable, V>
        implements ListObservable.ListObserver<Void> {
    /**
     * A generic view binder that associates a view with a model.
     * @param <M> The {@link ListObservable} model.
     * @param <V> The view or view holder that should be changed based on the model.
     */
    public interface ViewBinder<M, V> {
        void onItemsInserted(M model, V view, int index, int count);
        void onItemsRemoved(M model, V view, int index, int count);
        void onItemsChanged(M model, V view, int index, int count);
    }

    private final V mView;
    private final M mModel;
    private final ViewBinder<M, V> mViewBinder;

    /**
     * Construct a new ListModelChangeProcessor.
     * @param model The model containing the data to be bound.
     * @param view The view to which data will be bound.
     * @param viewBinder A class that binds the model to the view.
     */
    public ListModelChangeProcessor(M model, V view, ViewBinder<M, V> viewBinder) {
        mModel = model;
        mView = view;
        mViewBinder = viewBinder;
    }

    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        assert source == mModel;
        mViewBinder.onItemsInserted(mModel, mView, index, count);
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        assert source == mModel;
        mViewBinder.onItemsRemoved(mModel, mView, index, count);
    }

    @Override
    public void onItemRangeChanged(
            ListObservable source, int index, int count, @Nullable Void payload) {
        assert source == mModel;
        mViewBinder.onItemsChanged(mModel, mView, index, count);
    }
}