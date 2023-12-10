// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.modelutil;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;

/**
 * A model change processor (MCP), i.e. an implementation of {@link RecyclerViewAdapter.Delegate},
 * which represents a homogeneous {@link SimpleList} of items that are independent of each other.
 * It is intended primarily (but not exclusively) for use in a {@link RecyclerView}.
 * @param <T> The type of items in the list.
 * @param <VH> The view holder type that shows items.
 */
public class SimpleRecyclerViewMcp<T, VH> extends SimpleRecyclerViewMcpBase<T, VH, Void> {
    /**
     * View binding interface.
     * @param <T> The type of items in the list.
     * @param <VH> The view holder type that shows items.
     */
    public interface ViewBinder<T, VH> {
        /**
         * Called to display the specified {@code item} in the provided {@code holder}.
         * @param holder The view holder which should be updated to represent the {@code item}.
         * @param item The item in the list.
         */
        void onBindViewHolder(VH holder, T item);
    }

    /**
     * @param model The {@link SimpleList} model used to retrieve items to display.
     * @param itemViewTypeCallback The callback to return the view type for an item, or null to use
     *     the default view type.
     * @param viewBinder The {@link ViewBinder} binding this adapter to the view holder.
     */
    public SimpleRecyclerViewMcp(
            ListModel<T> model,
            @Nullable ItemViewTypeCallback<T> itemViewTypeCallback,
            ViewBinder<T, VH> viewBinder) {
        super(
                itemViewTypeCallback,
                (holder, item, payload) -> viewBinder.onBindViewHolder(holder, item),
                model);
    }
}
