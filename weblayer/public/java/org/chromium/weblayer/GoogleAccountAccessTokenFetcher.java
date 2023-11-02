// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

import java.util.Set;

/**
 * Used to fetch OAuth2 access tokens for the user's current GAIA account.
 * @since 89
 */
abstract class GoogleAccountAccessTokenFetcher {
    /**
     * Called when the WebLayer implementation wants to fetch an access token for the embedder's
     * current GAIA account (if any) and the given scopes. The client should invoke
     * |onTokenFetchedCallback| when its internal token fetch is complete, passing either the
     * fetched access token or the empty string in the case of failure (e.g., if there is no current
     * GAIA account or there was an error in the token fetch).
     *
     * NOTE: WebLayer will not perform any caching of the returned token but will instead make a new
     * request each time that it needs to use an access token. The expectation is that the client
     * will use caching internally to minimize latency of these requests.
     */
    public abstract void fetchAccessToken(
            @NonNull Set<String> scopes, @NonNull Callback<String> onTokenFetchedCallback);

    /**
     * Called when a token previously obtained via a call to fetchAccessToken(|scopes|) is
     * identified as invalid, so the embedder can take appropriate action (e.g., dropping the token
     * from its cache and/or force-fetching a new token).
     */
    public abstract void onAccessTokenIdentifiedAsInvalid(
            @NonNull Set<String> scopes, @NonNull String token);
}
