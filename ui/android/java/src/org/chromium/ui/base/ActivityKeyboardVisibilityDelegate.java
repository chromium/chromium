// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.view.View;

import org.chromium.base.BaseFeatureList;
import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.lang.ref.WeakReference;

/**
 * A {@link KeyboardVisibilityDelegate} that listens to a given activity for layout and keyboard
 * inset changes. It notifies {@link KeyboardVisibilityDelegate.KeyboardVisibilityListener} whenever
 * the layout or keyboard inset change is suspected to be related to a software keyboard changing
 * visibility.
 */
@NullMarked
public class ActivityKeyboardVisibilityDelegate extends KeyboardVisibilityDelegate
        implements View.OnLayoutChangeListener {
    private final Callback<Integer> mOnKeyboardInsetChanged = this::onKeyboardInsetChanged;

    private final WeakReference<@Nullable Activity> mActivity;
    private MonotonicObservableSupplier<Integer> mKeyboardInsetSupplier =
            ObservableSuppliers.alwaysNull();
    private boolean mIsKeyboardShowing;
    private @Nullable View mContentViewForTesting;

    /**
     * Creates a new delegate listening to the given activity. If the activity is destroyed, it will
     * continue to work as a regular {@link KeyboardVisibilityDelegate}.
     *
     * @param activity A {@link WeakReference} to an {@link Activity}.
     */
    public ActivityKeyboardVisibilityDelegate(WeakReference<@Nullable Activity> activity) {
        mActivity = activity;
    }

    /** Sets the keyboard inset supplier. */
    void setKeyboardInsetSupplier(MonotonicObservableSupplier<Integer> keyboardInsetSupplier) {
        if (hasKeyboardVisibilityListeners()) {
            mKeyboardInsetSupplier.removeObserver(mOnKeyboardInsetChanged);
        }
        mKeyboardInsetSupplier = keyboardInsetSupplier;
        if (hasKeyboardVisibilityListeners()) {
            mKeyboardInsetSupplier.addSyncObserverAndPostIfNonNull(mOnKeyboardInsetChanged);
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
        mIsKeyboardShowing = isKeyboardObscuring(content);
        content.addOnLayoutChangeListener(this);

        mKeyboardInsetSupplier.addSyncObserverAndPostIfNonNull(mOnKeyboardInsetChanged);
    }

    @Override
    public void unregisterKeyboardVisibilityCallbacks() {
        Activity activity = getActivity();
        if (activity != null) {
            getContentView(activity).removeOnLayoutChangeListener(this);
        }

        mKeyboardInsetSupplier.removeObserver(mOnKeyboardInsetChanged);
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
        updateKeyboardShowing(isKeyboardObscuring(getContentView(activity)));
    }

    private void onKeyboardInsetChanged(int inset) {
        if (!BaseFeatureList.sSplitScreenImeInsetVisibility.isEnabled()) {
            updateKeyboardShowing(inset > 0);
            return;
        }
        Activity activity = getActivity();
        if (activity == null) return;
        updateKeyboardShowing(isKeyboardObscuring(getContentView(activity)));
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
