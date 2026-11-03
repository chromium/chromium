// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.edge_to_edge;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.util.TokenHolder;

/**
 * Owns at most one edge-to-edge token from an {@link EdgeToEdgeStateProvider} and exposes simple
 * acquire/release operations so callers do not have to manage {@link TokenHolder#INVALID_TOKEN}
 * themselves.
 */
@NullMarked
public class EdgeToEdgeTokenHolder {
    private final EdgeToEdgeStateProvider mProvider;
    private int mToken = TokenHolder.INVALID_TOKEN;

    public EdgeToEdgeTokenHolder(EdgeToEdgeStateProvider provider) {
        mProvider = provider;
    }

    /** Acquires an edge-to-edge token if one is not already held. */
    public void acquireTokenIfEmpty() {
        if (mToken == TokenHolder.INVALID_TOKEN) {
            mToken = mProvider.acquireEdgeToEdgeToken();
        }
    }

    /** Releases the held token, if any. No-op when no token is held. */
    public void release() {
        if (mToken == TokenHolder.INVALID_TOKEN) return;
        mProvider.releaseEdgeToEdgeToken(mToken);
        mToken = TokenHolder.INVALID_TOKEN;
    }

    /** Returns whether an edge-to-edge token is currently held. */
    public boolean hasToken() {
        return mToken != TokenHolder.INVALID_TOKEN;
    }
}
