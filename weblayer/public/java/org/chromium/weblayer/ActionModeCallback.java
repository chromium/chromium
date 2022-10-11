// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * Used to override floating some action mode menu items.
 *
 * @since 88
 */
abstract class ActionModeCallback {
    /**
     * Called when an overridden item type is clicked. The action mode is closed after this returns.
     * @param selectedText the raw selected text. Client is responsible for trimming it to fit into
     *                     some use cases as the text can be very large.
     */
    public void onActionItemClicked(@ActionModeItemType int item, String selectedText) {}
}
