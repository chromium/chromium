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
    private final Callback<Integer> mOnKeyboardInsetChanged = (inset) -> updateKeyboardState();
    private final Callback<Boolean> mOnImeVisibilityChanged = (visible) -> updateKeyboardState();

    private final WeakReference<@Nullable Activity> mActivity;
    private MonotonicObservableSupplier<Integer> mKeyboardInsetSupplier =
            ObservableSuppliers.alwaysNull();
    private MonotonicObservableSupplier<Boolean> mImeVisibilitySupplier =
            ObservableSuppliers.alwaysNull();
    private boolean mIsKeyboardObscuring;
    private boolean mIsKeyboardVisible;
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
            updateKeyboardState();
        }
    }

    /** Sets the IME on-screen visibility supplier. */
    void setImeVisibilitySupplier(MonotonicObservableSupplier<Boolean> imeVisibilitySupplier) {
        if (hasKeyboardVisibilityListeners()) {
            mImeVisibilitySupplier.removeObserver(mOnImeVisibilityChanged);
        }
        mImeVisibilitySupplier = imeVisibilitySupplier;
        if (hasKeyboardVisibilityListeners()) {
            mImeVisibilitySupplier.addSyncObserverAndPostIfNonNull(mOnImeVisibilityChanged);
            updateKeyboardState();
        }
    }

    public @Nullable Activity getActivity() {
        return mActivity.get();
    }

    @Override
    public boolean isKeyboardVisible(View view) {
        return super.isKeyboardVisible(view) || isActivityImeVisible(view);
    }

    private boolean isActivityImeVisible(View view) {
        if (!BaseFeatureList.sSplitScreenImeInsetVisibility.isEnabled()) return false;
        Activity activity = getActivity();
        if (activity == null || activity.isFinishing() || activity.isDestroyed()) {
            return false;
        }
        View content = getContentView(activity);
        if (content == null || view.getRootView() != content.getRootView()) {
            return false;
        }
        return Boolean.TRUE.equals(mImeVisibilitySupplier.get());
    }

    @Override
    public void registerKeyboardVisibilityCallbacks() {
        Activity activity = getActivity();
        if (activity == null) return;
        View content = getContentView(activity);
        if (content == null) return;
        mIsKeyboardObscuring = isKeyboardObscuring(content);
        mIsKeyboardVisible = isKeyboardVisible(content);
        content.addOnLayoutChangeListener(this);

        mKeyboardInsetSupplier.addSyncObserverAndPostIfNonNull(mOnKeyboardInsetChanged);
        mImeVisibilitySupplier.addSyncObserverAndPostIfNonNull(mOnImeVisibilityChanged);
    }

    @Override
    public void unregisterKeyboardVisibilityCallbacks() {
        Activity activity = getActivity();
        if (activity != null) {
            View content = getContentView(activity);
            if (content != null) {
                content.removeOnLayoutChangeListener(this);
            }
        }

        mKeyboardInsetSupplier.removeObserver(mOnKeyboardInsetChanged);
        mImeVisibilitySupplier.removeObserver(mOnImeVisibilityChanged);
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
        updateKeyboardState();
    }

    private void updateKeyboardState() {
        Activity activity = getActivity();
        if (activity == null) return;
        View content = getContentView(activity);
        if (content == null) return;
        boolean isObscuring = isKeyboardObscuring(content);
        boolean isVisible = isKeyboardVisible(content);
        boolean obscuringChanged = isObscuring != mIsKeyboardObscuring;
        boolean visibleChanged = isVisible != mIsKeyboardVisible;
        mIsKeyboardObscuring = isObscuring;
        mIsKeyboardVisible = isVisible;
        if (obscuringChanged) {
            notifyListeners(isObscuring);
        }
        if (visibleChanged && mIsKeyboardVisible == isVisible) {
            notifyVisibilityListeners(isVisible);
        }
    }

    private @Nullable View getContentView(Activity activity) {
        if (mContentViewForTesting != null) return mContentViewForTesting;

        return activity.findViewById(android.R.id.content);
    }

    void setContentViewForTesting(View contentView) {
        mContentViewForTesting = contentView;
    }
}
