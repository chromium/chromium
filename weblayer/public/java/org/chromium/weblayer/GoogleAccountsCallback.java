// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

/**
 * Used to intercept interaction with GAIA accounts.
 */
abstract class GoogleAccountsCallback {
    /**
     * Called when a user wants to change the state of their GAIA account. This could be a signin,
     * signout, or any other action. See {@link GoogleAccountServiceType} for all the possible
     * actions.
     */
    public abstract void onGoogleAccountsRequest(@NonNull GoogleAccountsParams params);

    /**
     * The current GAIA ID the user is signed in with, or empty if the user is signed out. This can
     * be provided on a best effort basis if the ID is not available immediately.
     */
    public abstract @NonNull String getGaiaId();
}
