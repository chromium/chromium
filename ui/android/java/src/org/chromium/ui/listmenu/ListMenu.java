// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.view.View;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * Representation of a list menu. Contains and manages a content view by {@link #getContentView()}.
 * Handles click events of list items by {@link Delegate#onItemSelected(PropertyModel)}.
 */
public interface ListMenu {
    /** Delegate handling list item click event of {@link ListMenu}. */
    @FunctionalInterface
    interface Delegate {
        void onItemSelected(PropertyModel item);
    }

    /**
     * @return Content representing the list menu, containing a {@link android.widget.ListView}.
     */
    View getContentView();

    /**
     * @param runnable The task to be run on click event on content view.
     */
    void addContentViewClickRunnable(Runnable runnable);

    /**
     * @return Width of the largest item in the list.
     */
    int getMaxItemWidth();
}
