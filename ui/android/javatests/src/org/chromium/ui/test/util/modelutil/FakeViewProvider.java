// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util.modelutil;

import static org.junit.Assert.assertTrue;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.ui.ViewProvider;

/**
 * Fake view provider for tests.
 * @param <T> The view type.
 */
public class FakeViewProvider<T extends View> implements ViewProvider<T> {
    private final Promise<T> mViewPromise;
    private final Promise<T> mViewInflatedPromise = new Promise<>();
    private boolean mInflationStarted;

    /**
     * Creates a new instance without a view. The view should be passed in to
     * {@link #finishInflation} later to signal that inflation has finished.
     */
    public FakeViewProvider() {
        mViewPromise = new Promise<>();
    }

    /**
     * Creates a new instance with a view. Inflation will finish without requiring a call to
     * {@link #finishInflation}.
     * @param view The view to be provided.
     */
    public FakeViewProvider(T view) {
        mViewPromise = Promise.fulfilled(view);
    }

    public boolean inflationHasStarted() {
        return mInflationStarted;
    }

    /**
     * Finishes inflation with the given view. This method should only be called on instances
     * without a view, and after inflation has started. Clients that want to provide a view before
     * inflation has started should use the {@link #FakeViewProvider(T)} constructor.
     * @param view The view to be provided.
     * @see #inflationHasStarted
     */
    public void finishInflation(T view) {
        assertTrue(mInflationStarted);
        mViewPromise.fulfill(view);
    }

    @Override
    public void inflate() {
        mInflationStarted = true;
        mViewPromise.then(mViewInflatedPromise::fulfill);
    }

    @Override
    public void whenLoaded(Callback<T> callback) {
        mViewInflatedPromise.then(callback);
    }
}
