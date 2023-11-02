// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * An interface that allows clients to handle error page interactions.
 */
abstract class ErrorPageCallback {
    /**
     * The user has attempted to back out of an error page, such as one warning of an SSL error.
     *
     * @return true if the action was overridden and WebLayer should skip default handling.
     */
    public abstract boolean onBackToSafety();

    /**
     * Called when an error is encountered. A null return value results in a default error page
     * being shown, a non-null return value results in showing the content of the returned
     * {@link ErrorPage}.
     *
     * @param navigation The navigation that encountered the error.
     *
     * @return The error page.
     */
    public @Nullable ErrorPage getErrorPage(@NonNull Navigation navigation) {
        return null;
    }
}
