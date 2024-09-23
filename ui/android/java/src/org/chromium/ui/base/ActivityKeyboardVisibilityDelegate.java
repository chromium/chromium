// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.lang.ref.WeakReference;

/**
 * A {@link KeyboardVisibilityDelegate} that listens to a given activity for layout and keyboard
 * inset changes. It notifies {@link KeyboardVisibilityDelegate.KeyboardVisibilityListener} whenever
 * the layout or keyboard inset change is suspected to be related to a software keyboard changing
 * visibility.
 */
public class ActivityKeyboardVisibilityDelegate extends KeyboardVisibilityDelegate
        implements View.OnLayoutChangeListener {
    private final Callback<Integer> mOnKeyboardInsetChanged = this::onKeyboardInsetChanged;

    private WeakReference<Activity> mActivity;
    private LazyOneshotSupplier<ObservableSupplier<Integer>> mLazyKeyboardInsetSupplier;
    private boolean mIsKeyboardShowing;
    private View mContentViewForTesting;

    /**
     * Creates a new delegate listening to the given activity. If the activity is destroyed, it will
     * continue to work as a regular {@link KeyboardVisibilityDelegate}.
     *
     * @param activity A {@link WeakReference} to an {@link Activity}.
     */
    public ActivityKeyboardVisibilityDelegate(WeakReference<Activity> activity) {
        mActivity = activity;
    }

    /** Sets the keyboard inset supplier. */
    void setLazyKeyboardInsetSupplier(
            LazyOneshotSupplier<ObservableSupplier<Integer>> lazyKeyboardInsetSupplier) {
        assert lazyKeyboardInsetSupplier != null;
        assert mLazyKeyboardInsetSupplier == null;
        mLazyKeyboardInsetSupplier = lazyKeyboardInsetSupplier;
        if (hasKeyboardVisibilityListeners()) {
            mLazyKeyboardInsetSupplier.get().addObserver(mOnKeyboardInsetChanged);
        }
    }

    public @Nullable Activity getActivity() {
        return mActivity.get();
    }

    @Override
    public void registerKeyboardVisibilityCallbacks() {
        Activity activity = getActivity();
        if (activity == null) return;
        View content = getContentView(activity);
        mIsKeyboardShowing = isKeyboardShowing(activity, content);
        content.addOnLayoutChangeListener(this);

        if (mLazyKeyboardInsetSupplier == null) return;

        mLazyKeyboardInsetSupplier.get().addObserver(mOnKeyboardInsetChanged);
    }

    @Override
    public void unregisterKeyboardVisibilityCallbacks() {
        Activity activity = getActivity();
        if (activity == null) return;
        getContentView(activity).removeOnLayoutChangeListener(this);

        if (mLazyKeyboardInsetSupplier == null) return;

        mLazyKeyboardInsetSupplier.get().removeObserver(mOnKeyboardInsetChanged);
    }

    @Override
    public void onLayoutChange(
            View v,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        Activity activity = getActivity();
        if (activity == null) return;
        View content = getContentView(activity);
        updateKeyboardShowing(isKeyboardShowing(activity, content));
    }

    private void onKeyboardInsetChanged(int inset) {
        updateKeyboardShowing(inset > 0);
    }

    private void updateKeyboardShowing(boolean isShowing) {
        if (mIsKeyboardShowing == isShowing) return;
        mIsKeyboardShowing = isShowing;
        notifyListeners(isShowing);
    }

    private View getContentView(Activity activity) {
        if (mContentViewForTesting != null) return mContentViewForTesting;

        return activity.findViewById(android.R.id.content);
    }

    void setContentViewForTesting(View contentView) {
        mContentViewForTesting = contentView;
    }
}
