// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import java.util.concurrent.Future;

/**
 * ListenableResult implementation that also supports the Future interface.
 *
 * @param <V> The type of the computation's result.
 */
public abstract class ListenableFuture<V> extends ListenableResult<V> implements Future<V> {
    /* package */ ListenableFuture() {}

    @Override
    public V get() {
        if (getResult() != null) return getResult();
        onLoad();
        return getResult();
    }

    /**
     * Called to synchronously load the result. Implementation should call supplyResult().
     */
    /* package */ abstract void onLoad();

    @Override
    public boolean isDone() {
        return getResult() != null;
    }
}
