// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import androidx.annotation.Nullable;

/**
 * A model change processor for use with a {@link ListObservable} model. The
 * {@link ListModelChangeProcessor} should be registered as a list observer of the model.
 * Internally uses a view binder to bind model properties to a view like a TabLayout.
 *
 * Do not use this class to fill {@link androidx.recyclerview.widget.RecyclerView}s - consider using
 * the {@link SimpleRecyclerViewMcp} which was specifically designed for that use case!
 *
 * @param <M> The {@link ListObservable} model.
 * @param <V> The view object that is changing.
 * @param <P> The payload for partial updates. Void can be used if a payload is not needed.
 */
public class ListModelChangeProcessor<M extends ListObservable<P>, V, P>
        implements ListObservable.ListObserver<P> {
    /**
     * A generic view binder that associates a view with a model.
     *
     * Refer to Javadocs for {@link ListObservable} for more information on the methods.
     *
     * @param <M> The {@link ListObservable} model.
     * @param <V> The view or view holder that should be changed based on the model.
     * @param <P> The payload for partial updates. Void can be used if a payload is not needed.
     */
    public interface ViewBinder<M, V, P> {
        void onItemsInserted(M model, V view, int index, int count);

        void onItemsRemoved(M model, V view, int index, int count);

        void onItemsChanged(M model, V view, int index, int count, P payload);
    }

    private final V mView;
    private final M mModel;
    private final ViewBinder<M, V, P> mViewBinder;

    /**
     * Construct a new ListModelChangeProcessor.
     * @param model The model containing the data to be bound.
     * @param view The view to which data will be bound.
     * @param viewBinder A class that binds the model to the view.
     */
    public ListModelChangeProcessor(M model, V view, ViewBinder<M, V, P> viewBinder) {
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
            ListObservable<P> source, int index, int count, @Nullable P payload) {
        assert source == mModel;
        mViewBinder.onItemsChanged(mModel, mView, index, count, payload);
    }
}
