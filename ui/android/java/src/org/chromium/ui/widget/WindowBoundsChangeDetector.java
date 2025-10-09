// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.graphics.Rect;
import android.view.View;

import org.chromium.build.annotations.NullMarked;

/** A utility class to detect changes in a view's bounds. */
@NullMarked
public class WindowBoundsChangeDetector implements View.OnLayoutChangeListener {
    private final View mView;
    private final Runnable mOnBoundsChangedCallback;
    private final Rect mCachedBounds = new Rect();

    /**
     * Constructs a new {@link WindowBoundsChangeDetector}.
     *
     * @param view The view to observe for layout changes.
     * @param onBoundsChangedCallback The callback to run when the view's bounds change.
     */
    public WindowBoundsChangeDetector(View view, Runnable onBoundsChangedCallback) {
        mView = view;
        mOnBoundsChangedCallback = onBoundsChangedCallback;
        mView.addOnLayoutChangeListener(this);
        mCachedBounds.set(0, 0, view.getWidth(), view.getHeight());
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
        int width = right - left;
        int height = bottom - top;
        if (mCachedBounds.width() != width || mCachedBounds.height() != height) {
            mCachedBounds.set(0, 0, width, height);
            mOnBoundsChangedCallback.run();
        }
    }

    /** Stops observing the view for layout changes. */
    public void detach() {
        mView.removeOnLayoutChangeListener(this);
    }
}
