// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IdRes;
import androidx.annotation.LayoutRes;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * An implementation of ViewStub that inflates the view in a background thread. Callbacks are still
 * called on the UI thread.
 *
 * <p>Note: This class automatically supports AppCompat and Material Design view rewrites (e.g.,
 * converting {@code <Button>} to {@code <MaterialButton>} or {@code <AppCompatButton>}) by
 * dynamically loading the theme's configured {@link AppCompatLayoutInflater}.
 *
 * <p>However, executing complex widget constructor logic on a background thread is still **not 100%
 * thread-safe**. These constructors access shared static caches (like {@code
 * AppCompatDrawableManager}) and the Activity's {@link android.content.res.Resources.Theme}, which
 * can lead to subtle data races with the UI thread if the UI thread is concurrently active or if
 * multiple inflations occur (though the latter is mitigated by serialization).
 *
 * <p>Additionally, there is a risk associated with **configuration changes** (e.g., screen
 * rotation, locale updates, or dark mode toggles) occurring during background inflation. If a
 * configuration change happens while the background thread is inflating the layout:
 *
 * <ul>
 *   <li>The background thread may read resources using the stale configuration or contend with the
 *       UI thread as the {@link android.content.res.Resources} object is being updated, leading to
 *       data races.
 *   <li>The resulting inflated view may contain stale resources (e.g., wrong layout dimensions or
 *       incorrect localized strings) for the new configuration.
 *   <li>If the Activity is recreated, the finished inflation task might attempt to attach the view
 *       to a detached parent or a destroyed Activity context.
 * </ul>
 *
 * <p>For maximum safety when inflating asynchronously, prefer simple framework widgets and avoid
 * highly complex styling in the XML, or fall back to inflating on the UI thread.
 *
 * <p>TODO(crbug.com/40937701): Deprecate AsyncViewStub or make it per activity.
 */
@NullMarked
public class AsyncViewStub extends View {
    private static final String TAG = "AsyncViewStub";

    private @LayoutRes int mLayoutResource;
    private @IdRes int mInflatedId;
    private @Nullable View mInflatedView;

    private final ObserverList<Callback<View>> mListeners = new ObserverList<>();
    private boolean mOnBackground;

    public AsyncViewStub(Context context, AttributeSet attrs) {
        super(context, attrs);
        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.AsyncViewStub);
        mLayoutResource = a.getResourceId(R.styleable.AsyncViewStub_layout, 0);
        mInflatedId = a.getResourceId(R.styleable.AsyncViewStub_inflatedId, View.NO_ID);
        a.recycle();

        setVisibility(GONE);
        setWillNotDraw(true);
    }

    /**
     * Specifies the layout resource to inflate when {@link #inflate()} is invoked. The View created
     * by inflating the layout resource is used to replace this AsyncViewStub in its parent.
     *
     * @param layoutResource A valid layout resource identifier (different from 0.)
     */
    public void setLayoutResource(@LayoutRes int layoutResource) {
        mLayoutResource = layoutResource;
    }

    /**
     * Sets whether the view should be inflated on a background thread or the UI thread (the
     * default). This method should not be called after the view has been inflated.
     *
     * @param shouldInflateOnBackgroundThread True if the view should be inflated on a background
     *     thread, false otherwise.
     */
    public void setShouldInflateOnBackgroundThread(boolean shouldInflateOnBackgroundThread) {
        assert mInflatedView == null;
        mOnBackground = shouldInflateOnBackgroundThread;
    }

    /** Returns the id taken by the inflated view. */
    public @IdRes int getInflatedId() {
        return mInflatedId;
    }

    /**
     * Defines the id taken by the inflated view.
     *
     * @param inflatedId A positive integer used to identify the inflated view or {@link
     *     View#NO_ID}.
     */
    public void setInflatedId(@IdRes int inflatedId) {
        mInflatedId = inflatedId;
    }

    /**
     * Starts background inflation for the stub, the AsyncViewStub must be attached to the window
     * (ie have a parent) before you call inflate on it. Must be called on the UI thread.
     */
    public void inflate() {
        try (TraceEvent te = TraceEvent.scoped("AsyncViewStub.inflate")) {
            ThreadUtils.assertOnUiThread();
            ViewGroup viewParent = (ViewGroup) getParent();
            assert viewParent != null;
            AsyncLayoutInflater asyncLayoutInflater = new AsyncLayoutInflater(getContext());
            if (mOnBackground) {
                asyncLayoutInflater.inflate(mLayoutResource, viewParent, this::onInflateFinished);
            } else {
                View inflatedView = asyncLayoutInflater.inflateSync(mLayoutResource, viewParent);
                onInflateFinished(inflatedView);
            }
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        setMeasuredDimension(0, 0);
    }

    @SuppressLint("MissingSuperCall")
    @Override
    public void draw(Canvas canvas) {}

    @Override
    protected void dispatchDraw(Canvas canvas) {}

    private void onInflateFinished(View view) {
        if (mInflatedId != View.NO_ID) {
            view.setId(mInflatedId);
        }
        mInflatedView = view;
        replaceSelfWithView(view);
        callListeners(view);
    }

    private void callListeners(View view) {
        try (TraceEvent te = TraceEvent.scoped("AsyncViewStub.callListeners")) {
            ThreadUtils.assertOnUiThread();
            for (Callback<View> listener : mListeners) {
                listener.onResult(view);
            }
            mListeners.clear();
        }
    }

    /**
     * This should only be used by {@link AsyncViewProvider}, use {@link
     * AsyncViewProvider#whenLoaded} instead.
     *
     * Adds listener that gets called once the view is inflated and added to the view hierarchy. The
     * listeners are called on the UI thread. This method can only be called on the UI thread.
     *
     * @param listener the listener to add.
     */
    void addOnInflateListener(Callback<View> listener) {
        ThreadUtils.assertOnUiThread();
        if (mInflatedView != null) {
            listener.onResult(mInflatedView);
        } else {
            mListeners.addObserver(listener);
        }
    }

    /**
     * @return the inflated view or null if inflation is not complete yet.
     */
    @Nullable View getInflatedView() {
        return mInflatedView;
    }

    private void replaceSelfWithView(View view) {
        try (TraceEvent te = TraceEvent.scoped("AsyncViewStub.replaceSelfWithView")) {
            ViewGroup parent = (ViewGroup) getParent();
            if (parent == null) {
                // ViewStub was removed before inflation finished, so just no-op.
                return;
            }
            int index = parent.indexOfChild(this);
            parent.removeViewInLayout(this);
            ViewGroup.LayoutParams layoutParams = getLayoutParams();
            if (layoutParams != null) {
                parent.addView(view, index, layoutParams);
            } else {
                parent.addView(view, index);
            }
        }
    }
}
