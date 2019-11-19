// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import java.util.HashSet;
import java.util.Set;

/**
 * Helper class for holding tokens, useful when multiple entities need to manipulate the same
 * boolean state, e.g. visibility of a view.
 */
public class TokenHolder {
    /** An invalid token; this can be used to indicate no token is being held. */
    public static final int INVALID_TOKEN = -1;

    private int mNextToken;

    private final Set<Integer> mAcquiredTokens = new HashSet<>();
    private final Runnable mCallback;

    /**
     * @param callback is run when the token set becomes empty or non-empty.
     */
    public TokenHolder(Runnable callback) {
        mCallback = callback;
    }

    public int acquireToken() {
        int token = mNextToken++;
        mAcquiredTokens.add(token);
        if (mAcquiredTokens.size() == 1) mCallback.run();
        return token;
    }

    public void releaseToken(int token) {
        if (mAcquiredTokens.remove(token) && mAcquiredTokens.isEmpty()) {
            mCallback.run();
        }
    }

    public boolean hasTokens() {
        return !mAcquiredTokens.isEmpty();
    }

    public boolean containsOnly(int token) {
        return mAcquiredTokens.size() == 1 && mAcquiredTokens.contains(token);
    }
}
