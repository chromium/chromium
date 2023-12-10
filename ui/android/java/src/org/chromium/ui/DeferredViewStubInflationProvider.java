// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.view.View;
import android.view.ViewStub;

import org.chromium.base.Callback;
import org.chromium.base.Promise;

/**
 * View provider that inflates a ViewStub. This does not support inflation on a background thread,
 * therefore {@link AsyncViewProvider} should be preferred.
 * @param <T> The view type.
 */
public class DeferredViewStubInflationProvider<T extends View> implements ViewProvider<T> {
    private final ViewStub mViewStub;
    private Promise<T> mViewPromise = new Promise<>();

    @SuppressWarnings("unchecked")
    public DeferredViewStubInflationProvider(ViewStub viewStub) {
        assert viewStub != null : "ViewStub to inflate may not be null!";
        mViewStub = viewStub;
        mViewStub.setOnInflateListener(
                (stub, inflated) -> {
                    mViewPromise.fulfill((T) inflated);
                });
    }

    @Override
    public void inflate() {
        mViewStub.inflate();
    }

    @Override
    public void whenLoaded(Callback<T> callback) {
        if (mViewPromise.isFulfilled()) {
            callback.onResult(mViewPromise.getResult());
            return;
        }

        mViewPromise.then(callback);
    }
}
