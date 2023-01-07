// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.net.Uri;

import androidx.annotation.NonNull;

/**
 * Params passed to {@link GoogleAccountsCallback#onGoogleAccountsRequest}.
 */
class GoogleAccountsParams {
    /**
     * The requested service type such as "ADD_SESSION".
     */
    public final @GoogleAccountServiceType int serviceType;

    /**
     * The prefilled email. May be empty.
     */
    public final @NonNull String email;

    /**
     * The continue URL after the requested service is completed successfully. May be empty.
     */
    public final @NonNull Uri continueUri;

    /**
     * Whether the continue URL should be loaded in the same tab.
     */
    public final boolean isSameTab;

    public GoogleAccountsParams(@GoogleAccountServiceType int serviceType, String email,
            Uri continueUri, boolean isSameTab) {
        this.serviceType = serviceType;
        this.email = email;
        this.continueUri = continueUri;
        this.isSameTab = isSameTab;
    }
}
