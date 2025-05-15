// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.util.Pair;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.MVCListAdapter.ViewBuilder;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

import java.util.ArrayList;
import java.util.List;

/**
 * A generic adapter for any UI views inheriting from {@link ViewGroup} that can be bound to a
 * {@link ModelList} to dynamically display a list of views.
 *
 * <p>This class acts as a bridge between a list of data models (represented by {@link ModelList})
 * and a list of views, automatically creating, updating, and destroying views as the model list
 * changes.
 */
@NullMarked
public class ViewGroupAdapter implements Destroyable {
    @Nullable private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);

    /** The {@link ViewGroup} that hosts views. */
    private final ViewGroup mViewGroup;

    /** A map of view types to their corresponding {@link ViewBuilder} and {@link ViewBinder}. */
    private final SparseArray<Pair<ViewBuilder, ViewBinder>> mViewBuilderMap;

    /** The {@link ModelList} instance bound to this {@link ViewGroupAdapter}. */
    private final ModelList mModelList;

    /** An observer for the {@link ModelList} that updates the UI when the list changes. */
    private final ListObserver<Void> mModelObserver = new ModelObserver();

    /**
     * A list of {@link ViewHolder} instances, each managing a child view and its binding. The order
     * of elements in this list corresponds to the order of views in the view group.
     */
    private final List<ViewHolder> mChildren = new ArrayList<>();

    private ViewGroupAdapter(
            ViewGroup viewGroup,
            SparseArray<Pair<ViewBuilder, ViewBinder>> viewBuilderMap,
            ModelList modelList) {
        mViewGroup = viewGroup;
        mViewBuilderMap = viewBuilderMap;
        mModelList = modelList;

        mModelList.addObserver(mModelObserver);
        mModelObserver.onItemRangeInserted(mModelList, 0, mModelList.size());
    }

    /** Destroys the adapter, removing its observer from the {@link ModelList} and all views. */
    @Override
    public void destroy() {
        mModelList.removeObserver(mModelObserver);
        mViewGroup.removeAllViews();
        for (ViewHolder viewHolder : mChildren) {
            viewHolder.destroy();
        }
        mChildren.clear();
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }

    private void onItemInserted(int index, ListItem listItem) {
        ViewHolder viewHolder = createViewHolder(listItem.type);
        viewHolder.attachToModel(listItem.model);
        mChildren.add(index, viewHolder);
        mViewGroup.addView(viewHolder.getItemView(), index);
    }

    private void onItemRemoved(int index) {
        ViewHolder viewHolder = mChildren.remove(index);
        mViewGroup.removeView(viewHolder.getItemView());
        viewHolder.destroy();
    }

    private void onItemChanged(int index, ListItem listItem) {
        ViewHolder viewHolder = mChildren.get(index);
        // Fast path: if the type ID matches, reuse the view.
        if (viewHolder.getTypeId() == listItem.type) {
            viewHolder.attachToModel(listItem.model);
            return;
        }
        onItemRemoved(index);
        onItemInserted(index, listItem);
    }

    private void onItemMoved(int curIndex, int newIndex) {
        ViewHolder viewHolder = mChildren.remove(curIndex);
        mChildren.add(newIndex, viewHolder);
        mViewGroup.removeView(viewHolder.getItemView());
        mViewGroup.addView(viewHolder.getItemView(), newIndex);
    }

    private ViewHolder createViewHolder(int typeId) {
        var entry = mViewBuilderMap.get(typeId);
        if (entry == null) {
            throw new IllegalArgumentException("Unknown typeId: " + typeId);
        }
        // TODO: Reconsider hard-coded FrameLayout here. Do we want to make it
        // customizable, or get rid of it and make views direct children of
        // ViewGroupAdapter, similarly as RecyclerView?
        ViewGroup viewGroup = new FrameLayout(mViewGroup.getContext());
        View itemView = entry.first.buildView(viewGroup);
        return new ViewHolder(typeId, itemView, entry.second);
    }

    /** Builder class for creating {@link ViewGroupAdapter} instances. */
    public static class Builder {
        private final ViewGroup mViewGroup;
        private final ModelList mModelList;
        private final SparseArray<Pair<ViewBuilder, ViewBinder>> mViewBuilderMap =
                new SparseArray<>();

        /**
         * Constructs a new {@link Builder}.
         *
         * @param viewGroup The {@link ViewGroup} that will host the views.
         * @param modelList The {@link ModelList} that will provide the data.
         */
        public Builder(ViewGroup viewGroup, ModelList modelList) {
            mViewGroup = viewGroup;
            mModelList = modelList;
        }

        /**
         * Registers a view type with its corresponding builder and binder.
         *
         * <p>This method must be called for each distinct type of view that will be displayed in
         * the list.
         */
        public <T extends View> Builder registerType(
                int typeId,
                ViewBuilder<T> builder,
                ViewBinder<PropertyModel, T, PropertyKey> binder) {
            assert mViewBuilderMap.get(typeId) == null;
            mViewBuilderMap.put(typeId, new Pair<>(builder, binder));
            return this;
        }

        /**
         * Builds and returns a new {@link ViewGroupAdapter} instance configured with the registered
         * types and provided {@link ViewGroup} and {@link ModelList}.
         *
         * @return A new {@link ViewGroupAdapter}.
         */
        public ViewGroupAdapter build() {
            return new ViewGroupAdapter(mViewGroup, mViewBuilderMap, mModelList);
        }
    }

    /**
     * An inner class that acts as an observer for changes in the {@link ModelList}. This class
     * implements {@link ListObserver} and provides implementations for handling item insertions,
     * removals, changes, and moves by calling methods of the {@link ViewGroupAdapter} accordingly.
     */
    private class ModelObserver implements ListObserver<Void> {
        @Override
        public void onItemRangeInserted(ListObservable source, int index, int count) {
            assert mModelList != null;
            for (int i = 0; i < count; i++) {
                onItemInserted(index + i, mModelList.get(index + i));
            }
        }

        @Override
        public void onItemRangeRemoved(ListObservable source, int index, int count) {
            assert mModelList != null;
            for (int i = 0; i < count; i++) {
                onItemRemoved(index);
            }
        }

        @Override
        public void onItemRangeChanged(
                ListObservable source, int index, int count, @Nullable Void payload) {
            assert mModelList != null;
            for (int i = 0; i < count; i++) {
                onItemChanged(index + i, mModelList.get(index + i));
            }
        }

        @Override
        public void onItemMoved(ListObservable source, int curIndex, int newIndex) {
            assert mModelList != null;
            ViewGroupAdapter.this.onItemMoved(curIndex, newIndex);
        }
    }

    /**
     * A static inner class that acts as a ViewHolder for items in the {@link ViewGroupAdapter}.
     * Each {@link ViewHolder} manages a single child view and its binding to a {@link
     * PropertyModel}.
     */
    private static class ViewHolder implements Destroyable {
        /** The type ID of the view. */
        private final int mTypeId;

        /** The actual view managed by this ViewHolder. */
        private final View mItemView;

        /** The {@link ViewBinder} used to bind a {@link PropertyModel} to {@code mItemView}. */
        private final ViewBinder mViewBinder;

        /**
         * The {@link PropertyModelChangeProcessor} responsible for processing changes in the {@link
         * PropertyModel} and updating {@code mItemView}. Can be {@code null} if no model is
         * currently attached.
         */
        @Nullable
        private PropertyModelChangeProcessor<PropertyModel, View, PropertyKey>
                mModelChangeProcessor;

        /**
         * Constructs a new {@link ViewHolder}.
         *
         * @param typeId The type ID of the view.
         * @param itemView The view that this ViewHolder will manage.
         * @param viewBinder The {@link ViewBinder} to use for binding models to {@code itemView}.
         */
        public ViewHolder(int typeId, View itemView, ViewBinder viewBinder) {
            mTypeId = typeId;
            mItemView = itemView;
            mViewBinder = viewBinder;
        }

        public int getTypeId() {
            return mTypeId;
        }

        public View getItemView() {
            return mItemView;
        }

        /**
         * Attaches a {@link PropertyModel} to this ViewHolder's view. If a model is already
         * attached, it will be detached first. This method creates a {@link
         * PropertyModelChangeProcessor} to handle updates from the provided {@link PropertyModel}
         * to the {@code mItemView}.
         *
         * @param model The {@link PropertyModel} to attach.
         */
        public void attachToModel(PropertyModel model) {
            detachFromModel();
            mModelChangeProcessor =
                    PropertyModelChangeProcessor.create(model, mItemView, mViewBinder);
        }

        /**
         * Detaches the currently attached {@link PropertyModel} from this ViewHolder's view. This
         * method destroys the {@link PropertyModelChangeProcessor} to stop observing the model and
         * release resources.
         */
        public void detachFromModel() {
            if (mModelChangeProcessor != null) {
                mModelChangeProcessor.destroy();
                mModelChangeProcessor = null;
            }
        }

        /** Destroys this ViewHolder, detaching any attached model and releasing resources. */
        @Override
        public void destroy() {
            detachFromModel();
        }
    }
}
