// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * An interface that defined a simple API for list adapters in MVC. This class also contains a small
 * number of common utilities shared between implementations. In general, the only means of
 * interaction with these kinds of list adapters post-initialization should be to register the
 * different types of views that can be shown by the list, i.e.:
 * {@link #registerType(int, ViewBuilder, ViewBinder)}.
 */
public interface MVCListAdapter {
    /** A basic container for {@link PropertyModel}s with a type. */
    class ListItem {
        /** The type of view that the {@code model} is associated with. */
        public final int type;

        /** The model to be managed by a list. */
        public final PropertyModel model;

        /**
         * Build a new item to managed by a {@link ModelListAdapter}.
         * @param type The view type the model will bind to.
         * @param model The model that will be bound to a view.
         */
        public ListItem(int type, PropertyModel model) {
            this.type = type;
            this.model = model;
        }
    }

    /** A basic observable list for this adapter to use. This more or less acts as a typedef. */
    class ModelList extends ListModelBase<ListItem, Void> {}

    /**
     * An interface to provide a means to build specific view types.
     * @param <T> The type of view that the implementor will build.
     */
    interface ViewBuilder<T extends View> {
        /**
         * @param parent Parent view.
         * @return A new view to show in the list.
         */
        T buildView(@NonNull ViewGroup parent);
    }

    /**
     * Register a new view type that this adapter knows how to show.
     * @param typeId The ID of the view type. This should not match any other view type registered
     *               in this adapter.
     * @param builder A mechanism for building new views of the specified type.
     * @param binder A means of binding a model to the provided view.
     */
    <T extends View> void registerType(
            int typeId, ViewBuilder<T> builder, ViewBinder<PropertyModel, T, PropertyKey> binder);
}
