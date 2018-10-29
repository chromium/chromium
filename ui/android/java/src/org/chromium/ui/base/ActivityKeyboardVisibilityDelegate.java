// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.app.Activity;
import android.support.annotation.Nullable;
import android.view.View;

import org.chromium.ui.KeyboardVisibilityDelegate;

import java.lang.ref.WeakReference;

/**
 * A {@link KeyboardVisibilityDelegate} that listens to a given activity for layout changes. It
 * notifies {@link KeyboardVisibilityDelegate.KeyboardVisibilityListener} whenever the layout change
 * is suspected to be caused by a keyboard.
 */
public class ActivityKeyboardVisibilityDelegate
        extends KeyboardVisibilityDelegate implements View.OnLayoutChangeListener {
    private boolean mIsKeyboardShowing;
    private WeakReference<Activity> mActivity;

    /**
     * Creates a new delegate listening to the given activity. If the activity is destroyed, it will
     * continue to work as a regular {@link KeyboardVisibilityDelegate}.
     * @param activity A {@link WeakReference} to an {@link Activity}.
     */
    public ActivityKeyboardVisibilityDelegate(WeakReference<Activity> activity) {
        mActivity = activity;
    }

    public @Nullable Activity getActivity() {
        return mActivity.get();
    }

    @Override
    public void registerKeyboardVisibilityCallbacks() {
        Activity activity = getActivity();
        if (activity == null) return;
        View content = activity.findViewById(android.R.id.content);
        mIsKeyboardShowing = isKeyboardShowing(activity, content);
        content.addOnLayoutChangeListener(this);
    }

    @Override
    public void unregisterKeyboardVisibilityCallbacks() {
        Activity activity = getActivity();
        if (activity == null) return;
        activity.findViewById(android.R.id.content).removeOnLayoutChangeListener(this);
    }

    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        Activity activity = getActivity();
        if (activity == null) return;
        boolean isShowing = isKeyboardShowing(activity, v);
        if (mIsKeyboardShowing == isShowing) return;
        mIsKeyboardShowing = isShowing;
        notifyListeners(isShowing);
    }
}