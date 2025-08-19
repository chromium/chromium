// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.theme;

import android.content.Context;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.content.res.Resources.Theme;
import android.view.ContextThemeWrapper;

import androidx.annotation.StyleRes;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;

/**
 * Delegate class that used to provide themes and resources based on state. This class internally
 * holds a {@link android.view.ContextThemeWrapper}, and provide theme / resources based on current
 * state whether the overlay is enabled.
 *
 * <p>This class is useful when the feature wants to provide different sets of themes / resources in
 * different conditions, essentially allowing a ContextThemeWrapper to "unset" the applied theme
 * overlay. See {@link ThemeResourceWrapperJavaUnitTest} for example usage.
 *
 * <p>This class should only be accessed on the UI thread.
 */
@NullMarked
public class ThemeResourceWrapper {

    /** Observe the theme resource changes provided by the wrapper. */
    public interface ThemeObserver {

        /**
         * Called when the theme / resource is changed from the source.
         * */
        void onThemeResourceChanged(ThemeResourceWrapper source);
    }

    private final Context mBaseContext;
    private final @StyleRes int mResourceId;
    private final ObserverList<ThemeObserver> mObservers = new ObserverList<>();
    private Context mThemedContext;
    private boolean mIsUsingOverlay;

    // This is important so that we lock the call to mThemedContext. This is especially important
    // to avoid unlimited recursion.
    private boolean mIsBusy;

    /**
     * Create the instance based on the base context.
     *
     * @param baseContext The base context to be wrapped
     * @param resourceId The theme overlay resource to be used for the overlay.
     */
    public ThemeResourceWrapper(Context baseContext, @StyleRes int resourceId) {
        mBaseContext = baseContext;
        mResourceId = resourceId;
        mIsUsingOverlay = false;

        updateThemedContext();
    }

    /** Set whether we should enable the current theme overlay. */
    public void setIsUsingOverlay(boolean isUsingOverlay) {
        if (mIsUsingOverlay == isUsingOverlay) return;
        mIsUsingOverlay = isUsingOverlay;
        updateThemedContext();
    }

    /**
     * Return whether this class is reading resource from Themed context. This is a critical signal
     * when the mBaseContext is a ContextWrapper which delegates the calls (e.g. {@link #getTheme}
     * to a {@link ThemeResourceWrapper} instance. The delegating activity should reading this
     * signal to prevent recursion calls that causes stackoverflow.
     */
    public boolean isBusy() {
        return mIsBusy;
    }

    /**
     * Forward the #getTheme call to the theme wrapper or base context based on state.
     *
     * @see Context#getTheme()
     */
    public Theme getTheme() {
        assert !mIsBusy;
        try {
            mIsBusy = true;
            return mThemedContext.getTheme();
        } finally {
            mIsBusy = false;
        }
    }

    /**
     * Forward the #getResources call to the theme wrapper or base context based on state.
     *
     * @see Context#getResources()
     */
    public Resources getResources() {
        assert !mIsBusy;
        try {
            mIsBusy = true;
            return mThemedContext.getResources();
        } finally {
            mIsBusy = false;
        }
    }

    /**
     * Forward the #getResources call to the theme wrapper or base context based on state.
     *
     * @see Context#getResources()
     */
    public AssetManager getAssets() {
        assert !mIsBusy;
        try {
            mIsBusy = true;
            return mThemedContext.getAssets();
        } finally {
            mIsBusy = false;
        }
    }

    /**
     * Forward the #getSystemService call to the theme wrapper or base context based on state.
     *
     * @see Context#getSystemService(String)
     */
    public Object getSystemService(String name) {
        assert !mIsBusy;
        try {
            mIsBusy = true;
            return mThemedContext.getSystemService(name);
        } finally {
            mIsBusy = false;
        }
    }

    public void addObserver(ThemeObserver observer) {
        mObservers.addObserver(observer);
    }

    public void removeObserver(ThemeObserver observer) {
        mObservers.removeObserver(observer);
    }

    public void destroy() {
        mObservers.clear();
    }

    private void updateThemedContext() {
        mThemedContext =
                mIsUsingOverlay ? new ContextThemeWrapper(mBaseContext, mResourceId) : mBaseContext;
        for (ThemeObserver observer : mObservers) {
            observer.onThemeResourceChanged(this);
        }
    }

    Context getThemedContextForTesting() {
        return mThemedContext;
    }

    public boolean getIsUsingOverlayForTesting() {
        return mIsUsingOverlay;
    }
}
