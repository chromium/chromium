// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * Used for handling requests to open new tabs that are not associated with any existing tab.
 *
 * This will be used in cases where a service worker tries to open a document, e.g. via the Web API
 * clients.openWindow.
 *
 * @since 91
 */
abstract class OpenUrlCallback {
    /**
     * Called to get the {@link Browser} in which to create a new tab for a requested navigation.
     *
     * @return The {@link Browser} to host the new tab in, or null if the request should be
     *         rejected.
     */
    public abstract @Nullable Browser getBrowserForNewTab();

    /**
     * Called when a new tab has been created and added to the browser given by {@link
     * getBrowserForNewTab()}.
     *
     * It's expected this tab will be set to active.
     *
     * @param tab The new tab.
     */
    public abstract void onTabAdded(@NonNull Tab tab);
}
