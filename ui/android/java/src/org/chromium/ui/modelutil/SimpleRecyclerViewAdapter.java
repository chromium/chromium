// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.util.Pair;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * A simple adapter for {@link RecyclerView}. This is the RecyclerView version of the
 * {@link ModelListAdapter} with the API surfaces being identical in terms of managing items in the
 * view. In summary, use {@link #registerType(int, ViewBuilder, ViewBinder)} to tell the list
 * adapter how to display a particular item. Updates to the {@link ListObservable} list provided in
 * the constructor will immediately be reflected in the list.
 */
public class SimpleRecyclerViewAdapter
        extends RecyclerView.Adapter<SimpleRecyclerViewAdapter.ViewHolder>
        implements MVCListAdapter {
    /**
     * A simple {@link ViewHolder} that keeps a view, view binder, and an MCP that relate the two.
     */
    public static class ViewHolder extends RecyclerView.ViewHolder {
        /** The model change processor currently associated with this view and model. */
        private PropertyModelChangeProcessor<PropertyModel, View, PropertyKey> mCurrentMcp;

        /** The view binder that knows how to apply a model to the view this holder owns. */
        private ViewBinder<PropertyModel, View, PropertyKey> mBinder;

        /** A handle to the model currently held by this view holder. */
        public PropertyModel model;

        /**
         * @param itemView The view to manage.
         * @param binder The binder that knows how to apply a model to the view.
         */
        public ViewHolder(View itemView, ViewBinder<PropertyModel, View, PropertyKey> binder) {
            super(itemView);
            mBinder = binder;
        }

        /**
         * Set the model for this view holder to manage.
         * @param model The model that should be bound to the view.
         */
        void setModel(PropertyModel model) {
            if (mCurrentMcp != null) mCurrentMcp.destroy();
            this.model = model;
            if (this.model == null) return;
            mCurrentMcp = PropertyModelChangeProcessor.create(model, itemView, mBinder);
        }
    }

    /** The data that is shown in the list. */
    protected final ModelList mListData;

    /** The observer that watches the data for changes. */
    private final ListObserver<Void> mListObserver;

    /** A map of view types to view binders. */
    private final SparseArray<Pair<ViewBuilder, ViewBinder>> mViewBuilderMap = new SparseArray<>();

    public SimpleRecyclerViewAdapter(ModelList data) {
        mListData = data;
        mListObserver =
                new ListObserver<Void>() {
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
                            ListObservable<Void> source,
                            int index,
                            int count,
                            @Nullable Void payload) {
                        notifyItemRangeChanged(index, count);
                    }

                    @Override
                    public void onItemMoved(ListObservable source, int curIndex, int newIndex) {
                        notifyItemMoved(curIndex, newIndex);
                    }
                };
        mListData.addObserver(mListObserver);
    }

    /** Clean up any state that needs to be. */
    public void destroy() {
        mListData.removeObserver(mListObserver);
    }

    /** @return The ModelList describing RecyclerView children. */
    public ModelList getModelList() {
        return mListData;
    }

    @Override
    public int getItemCount() {
        return mListData.size();
    }

    @Override
    public <T extends View> void registerType(
            int typeId, ViewBuilder<T> builder, ViewBinder<PropertyModel, T, PropertyKey> binder) {
        assert mViewBuilderMap.get(typeId) == null;
        mViewBuilderMap.put(typeId, new Pair<>(builder, binder));
    }

    @Override
    public int getItemViewType(int position) {
        return mListData.get(position).type;
    }

    /**
     * Create a new view of the desired type.
     *
     * @param parent Parent view.
     * @param typeId Type of the view to create.
     * @return Created view.
     */
    protected View createView(ViewGroup parent, int typeId) {
        return mViewBuilderMap.get(typeId).first.buildView(parent);
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        return new ViewHolder(createView(parent, viewType), mViewBuilderMap.get(viewType).second);
    }

    @Override
    public void onBindViewHolder(ViewHolder viewHolder, int position) {
        viewHolder.setModel(mListData.get(position).model);
    }

    @Override
    public void onViewRecycled(ViewHolder holder) {
        holder.setModel(null);
    }
}
