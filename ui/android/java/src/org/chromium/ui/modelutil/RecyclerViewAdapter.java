// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import java.util.List;

/**
 * A base {@link RecyclerView} adapter that delegates most of its logic. This allows compositing
 * different delegates together to support different UI features living in the same {@link
 * RecyclerView}.
 *
 * @param <VH> The {@link ViewHolder} type for the {@link RecyclerView}.
 * @param <P> The payload type for partial updates, or {@link Void} if the adapter does not support
 *     partial updates.
 */
public class RecyclerViewAdapter<VH extends ViewHolder, P> extends RecyclerView.Adapter<VH>
        implements ListObservable.ListObserver<P> {
    /**
     * Delegate interface for the adapter.
     * @param <VH> The {@link ViewHolder} type for the {@link RecyclerView}.
     * @param <P> The payload type for partial updates, or {@link Void} if the adapter does not
     * support partial updates.
     */
    public interface Delegate<VH, P> extends ListObservable<P> {
        /**
         * Returns the number of items under this subtree. This method may be called
         * before initialization.
         *
         * @return The number of items under this subtree.
         * @see RecyclerView.Adapter#getItemCount()
         */
        int getItemCount();

        /**
         * @param position The position to query
         * @return The view type of the item at {@code position} under this subtree.
         * @see RecyclerView.Adapter#getItemViewType
         */
        int getItemViewType(int position);

        /**
         * Bind a given {@code viewHolder} to the data represented by the item at the given
         * {@code position} in the adapter. If {@code payload} is non-null, performs a "partial"
         * update of only the property represented by {@code payload}.
         * @param viewHolder A view holder to bind.
         * @param position The adapter position of the item to bind to the {@code viewHolder}.
         * @param payload The payload for partial updates, or null to perform a full bind.
         * @see RecyclerView.Adapter#onBindViewHolder
         */
        void onBindViewHolder(VH viewHolder, int position, @Nullable P payload);

        /**
         * Called when the {@link View} owned by {@code viewHolder} no longer needs to be attached
         * to its parent {@link RecyclerView}.  This is a good place to free up expensive resources
         * that might be owned by the particular {@link View} or {@code viewHolder}.
         * @param viewHolder A view holder that will be recycled.
         * @see RecyclerView.Adapter#onViewRecycled(ViewHolder)
         */
        default void onViewRecycled(VH viewHolder) {}

        /**
         * Describe the item at the given {@code position}. As the description is used in tests for
         * dumping state and equality checks, different items should have distinct descriptions,
         * but for items that are unique or don't have interesting state it can be sufficient to
         * return e.g. a string that describes the type of the item.
         * @param position The position of the item to be described.
         * @return A string description of the item.
         */
        default String describeItemForTesting(int position) {
            return "Unknown item at position " + position;
        }
    }

    /**
     * Factory for creating new {@link ViewHolder}s.
     * @param <VH> The {@link ViewHolder} type for the {@link RecyclerView}.
     */
    public interface ViewHolderFactory<VH> {
        /**
         * Called when the {@link RecyclerView} needs a new {@link ViewHolder} of the given
         * {@code viewType} to represent an item.
         *
         * @param parent The {@link ViewGroup} into which the new view will be added after
         *               it's bound to an adapter position.
         * @param viewType The view type of the new view.
         * @return A new {@link ViewHolder} that holds a view of the given view type.
         * @see RecyclerView.Adapter#createViewHolder
         */
        VH createViewHolder(ViewGroup parent, int viewType);
    }

    /**
     * Creates a new adapter for the given {@code delegate} and {@link ViewHolder} {@code factory}.
     * @param delegate The delegate for this adapter.
     * @param factory The {@link ViewHolder} factory for this adapter.
     */
    public RecyclerViewAdapter(Delegate<VH, P> delegate, ViewHolderFactory<VH> factory) {
        mDelegate = delegate;
        mFactory = factory;
        mDelegate.addObserver(this);
    }

    private final Delegate<VH, P> mDelegate;
    private final ViewHolderFactory<VH> mFactory;

    @Override
    public int getItemCount() {
        return mDelegate.getItemCount();
    }

    @Override
    public int getItemViewType(int position) {
        return mDelegate.getItemViewType(position);
    }

    @Override
    public VH onCreateViewHolder(ViewGroup parent, int viewType) {
        return mFactory.createViewHolder(parent, viewType);
    }

    @Override
    public void onBindViewHolder(VH vh, int position) {
        mDelegate.onBindViewHolder(vh, position, null);
    }

    @SuppressWarnings("unchecked")
    @Override
    public void onBindViewHolder(VH holder, int position, List<Object> payloads) {
        if (payloads.isEmpty()) {
            onBindViewHolder(holder, position);
            return;
        }

        for (Object p : payloads) {
            mDelegate.onBindViewHolder(holder, position, (P) p);
        }
    }

    @Override
    public void onViewRecycled(VH holder) {
        mDelegate.onViewRecycled(holder);
    }

    @Override
    public void onItemRangeInserted(ListObservable source, int index, int count) {
        notifyItemRangeInserted(index, count);
    }

    @Override
    public void onItemRangeRemoved(ListObservable source, int index, int count) {
        notifyItemRangeRemoved(index, count);
    }

    @Override
    public void onItemRangeChanged(
            ListObservable<P> source, int index, int count, @Nullable P payload) {
        notifyItemRangeChanged(index, count, payload);
    }

    @Override
    public void onItemMoved(ListObservable source, int curIndex, int newIndex) {
        notifyItemMoved(curIndex, newIndex);
    }
}
