// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.NonNull;

import java.util.ArrayList;

/**
 * Represents result of async computation which can be retrieved from a callback.
 * @param <V> The type of the computation's result.
 */
public class ListenableResult<V> {
    private boolean mHasResult;
    private V mResult;
    private final ArrayList<Callback<V>> mCallbacks = new ArrayList<>();

    /* package */ ListenableResult() {}

    /**
     * Call the callback with the result of computation.
     * Note callback may be called immediately.
     */
    public void addCallback(@NonNull Callback<V> callback) {
        if (mHasResult) {
            callback.onResult(mResult);
            return;
        }
        mCallbacks.add(callback);
    }

    /* package */ void supplyResult(V result) {
        assert !mHasResult;
        mResult = result;
        mHasResult = true;

        for (Callback<V> callback : mCallbacks) {
            callback.onResult(mResult);
        }
        mCallbacks.clear();
    }

    /* package */ V getResult() {
        return mResult;
    }
}
