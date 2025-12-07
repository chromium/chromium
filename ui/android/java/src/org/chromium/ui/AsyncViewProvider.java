// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A provider that encapsulates a {@link View} that is in the view hierarchy to be inflated by
 * an {@link AsyncViewStub}.
 * @param <T> type of the {@link View} that this provider encapsulates.
 */
@NullMarked
public class AsyncViewProvider<T extends View> implements Callback<View>, ViewProvider<T> {
    private int mResId;
    // Exactly one of mView and mViewStub is non-null at any point.
    private @Nullable T mView;
    private @Nullable AsyncViewStub mViewStub;
    private boolean mDestroyed;

    private AsyncViewProvider(AsyncViewStub viewStub, int resId) {
        mResId = resId;
        mViewStub = viewStub;
    }

    @SuppressWarnings("unchecked")
    private AsyncViewProvider(View view) {
        assert view != null;
        mView = (T) view;
    }

    /**
     * Returns a provider for a view in the view hierarchy that is to be inflated by {@param
     * viewStub}.
     * @param viewStub the {@link AsyncViewStub} that will inflate the view hierarchy containing the
     *                 {@link View}.
     * @param resId The resource id of the view that this provider should provide/encapsulate.
     * @return an {@link AsyncViewProvider} that encapsulates a view with id {@param resId}.
     */
    public static <E extends View> AsyncViewProvider<E> of(AsyncViewStub viewStub, int resId) {
        ThreadUtils.assertOnUiThread();
        View inflatedView = viewStub.getInflatedView();
        if (inflatedView != null) {
            return new AsyncViewProvider<>(inflatedView.findViewById(resId));
        }
        AsyncViewProvider<E> provider = new AsyncViewProvider<>(viewStub, resId);
        viewStub.addOnInflateListener(provider);
        return provider;
    }

    /**
     * Get a provider for a view with id {@param viewResId} that is (or going to be) in the view
     * hierarchy inflated by the AsyncViewStub with id {@param viewStubResId}.
     * @param root the {@link View} to use as the context for finding the View/ViewStub that the
     *             provider encapsulates.
     * @param viewStubResId the resource id of the AsyncViewStub that inflates the view hierarchy
     *                      where the encapsulated View lives.
     * @param viewResId the resource id of the view that the provider should provide/encapsulate.
     * @return an {@link AsyncViewProvider} that encapsulates a view with id {@param viewResId}.
     */
    public static <E extends View> AsyncViewProvider<E> of(
            View root, int viewStubResId, int viewResId) {
        ThreadUtils.assertOnUiThread();
        View viewStub = root.findViewById(viewStubResId);
        if (viewStub != null && viewStub instanceof AsyncViewStub) {
            // view stub not yet inflated
            return of((AsyncViewStub) viewStub, viewResId);
        }
        // view stub already inflated, return pre-loaded provider
        return new AsyncViewProvider<>(root.findViewById(viewResId));
    }

    @Override
    public void onResult(View view) {
        mView = view.findViewById(mResId);
        mViewStub = null;
    }

    /**
     * @return the {@link View} encapsulated by this provider or null (if the view has not been
     * inflated yet).
     */
    public @Nullable T get() {
        return mView;
    }

    /**
     * @param resId resource id of the {@link View} that the returned provider would
     *              encapsulate.
     * @param <E> type of the {@link View} that the returned provider would encapsulate
     * @return a provider for a {@link View} with resource id {@param resId} that is in the view
     * hierarchy of the {@link View} encapsulated by this provider.
     */
    public <E extends View> AsyncViewProvider<E> getChildProvider(int resId) {
        if (mView != null) {
            return new AsyncViewProvider<>(mView.findViewById(resId));
        }
        return of(assumeNonNull(mViewStub), resId);
    }

    @Override
    public void inflate() {
        assumeNonNull(mViewStub).inflate();
    }

    @Override
    public void whenLoaded(Callback<T> callback) {
        ThreadUtils.assertOnUiThread();
        if (mDestroyed) return;
        if (mView != null) {
            // fire right now if view already inflated.
            callback.onResult(mView);
        } else {
            assumeNonNull(mViewStub)
                    .addOnInflateListener(
                            (View view) -> {
                                if (mDestroyed) return;
                                // listeners are called in order so mView should be set correctly at
                                // this
                                // point.
                                callback.onResult(assumeNonNull(mView));
                            });
        }
    }

    /**
     * Same as {@link #destroy()} but takes a callback that is ensured to be run (either immediately
     * if the view is already inflated or after inflation of the {@link AsyncViewStub})).
     */
    public void destroy(Callback<T> destroyCallback) {
        mDestroyed = true;
        if (mView != null) {
            destroyCallback.onResult(mView);
            mView = null;
        }
        if (mViewStub != null) {
            mViewStub.addOnInflateListener(
                    (View view) -> {
                        destroyCallback.onResult(assumeNonNull(mView));
                    });
            mViewStub = null;
        }
    }
}
