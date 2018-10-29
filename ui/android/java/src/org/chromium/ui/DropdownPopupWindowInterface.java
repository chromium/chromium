// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.view.View;
import android.widget.AdapterView;
import android.widget.ListAdapter;
import android.widget.ListView;
import android.widget.PopupWindow;

import org.chromium.base.VisibleForTesting;

/**
 * The interface for dropdown popup window.
 */
@VisibleForTesting
public interface DropdownPopupWindowInterface {
    /**
     * Sets the adapter that provides the data and the views to represent the data
     * in this popup window.
     *
     * @param adapter The adapter to use to create this window's content.
     */
    void setAdapter(ListAdapter adapter);

    /**
     * Sets the initial selection.
     *
     * @param initialSelection The index of the initial item to select.
     */
    void setInitialSelection(int initialSelection);

    /**
     * Shows the popup. The adapter should be set before calling this method.
     */
    void show();

    /**
     * Set a listener to receive a callback when the popup is dismissed.
     *
     * @param listener Listener that will be notified when the popup is dismissed.
     */
    void setOnDismissListener(PopupWindow.OnDismissListener listener);

    /**
     * Sets the text direction in the dropdown. Should be called before show().
     * @param isRtl If true, then dropdown text direction is right to left.
     */
    void setRtl(boolean isRtl);

    /**
     * Disable hiding on outside tap so that tapping on a text input field associated with the popup
     * will not hide the popup.
     */
    void disableHideOnOutsideTap();

    /**
     * Sets the content description to be announced by accessibility services when the dropdown is
     * shown.
     * @param description The description of the content to be announced.
     */
    void setContentDescriptionForAccessibility(CharSequence description);

    /**
     * Sets a listener to receive events when a list item is clicked.
     *
     * @param clickListener Listener to register.
     */
    void setOnItemClickListener(AdapterView.OnItemClickListener clickListener);

    /**
     * Adds a non-scrolling View beneath the list. This View will be separated from the main list
     * by a single divider.
     * TODO(crbug.com/896349): This currently only works when called before show().
     */
    void setFooterView(View footerView);

    /**
     * Show the popup. Will have no effect if the popup is already showing.
     * Post a {@link #show()} call to the UI thread.
     */
    void postShow();

    /**
     * Disposes of the popup window.
     */
    void dismiss();

    /**
     * @return The {@link ListView} displayed within the popup window.
     */
    ListView getListView();

    /**
     * @return Whether the popup is currently showing.
     */
    boolean isShowing();
}
