// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

/**
 * A model change processor (MCP), i.e. an implementation of {@link RecyclerViewAdapter.Delegate},
 * which represents a homogeneous {@link SimpleList} of items that are independent of each other. It
 * is intended primarily (but not exclusively) for use in a {@link RecyclerView}.
 *
 * @param <T> The type of items in the list.
 * @param <VH> The view holder type that shows items.
 * @param <P> The payload type for partial updates. If the model change processor doesn't support
 *     partial updates, use the {@link SimpleRecyclerViewMcp} subclass.
 */
public class SimpleRecyclerViewMcpBase<T, VH, P> extends ForwardingListObservable<P>
        implements RecyclerViewAdapter.Delegate<VH, P> {
    private final SimpleList<T> mModel;
    private final ItemViewTypeCallback<T> mItemViewTypeCallback;
    private final ViewBinder<T, VH, P> mViewBinder;

    public SimpleRecyclerViewMcpBase(
            @Nullable ItemViewTypeCallback<T> itemViewTypeCallback,
            ViewBinder<T, VH, P> viewBinder,
            ListModelBase<T, P> model) {
        mItemViewTypeCallback = itemViewTypeCallback;
        mViewBinder = viewBinder;
        mModel = model;
        model.addObserver(this);
    }

    @Override
    public int getItemCount() {
        return mModel.size();
    }

    @Override
    public int getItemViewType(int position) {
        if (mItemViewTypeCallback == null) return 0;

        return mItemViewTypeCallback.getItemViewType(mModel.get(position));
    }

    @Override
    public void onBindViewHolder(VH holder, int position, @Nullable P payload) {
        mViewBinder.onBindViewHolder(holder, mModel.get(position), payload);
    }

    /**
     * A view binder used to bind items in the {@link ListObservable} model to view holders.
     *
     * @param <T> The item type in the {@link SimpleList} model.
     * @param <VH> The view holder type that shows items.
     * @param <P> The payload type for partial updates, or {@link Void} if the object doesn't
     * support partial updates.
     */
    public interface ViewBinder<T, VH, P> {
        /**
         * Called to display the specified {@code item} in the provided {@code holder}.
         * @param holder The view holder which should be updated to represent the {@code item}.
         * @param item The item in the list.
         * @param payload The payload for partial updates.
         */
        void onBindViewHolder(VH holder, T item, @Nullable P payload);
    }

    /**
     * A functional interface to return the view type for an item.
     * @param <T> The item type.
     */
    public interface ItemViewTypeCallback<T> {
        /**
         * @param item The item for which to return the view type.
         * @return The view type for the given {@code item}.
         * @see RecyclerView.Adapter#getItemViewType
         */
        int getItemViewType(T item);
    }
}
