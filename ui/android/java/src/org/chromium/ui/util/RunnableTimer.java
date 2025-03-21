// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.os.Handler;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A timer that executes a {@link Runnable} after a specified duration. The timer can be reset to
 * postpone the execution or cancelled to prevent it.
 */
@NullMarked
public class RunnableTimer {
    private long mDuration;
    private @Nullable Runnable mRunnableOnTimeUp;
    private Handler mAutoDismissTimer;

    public RunnableTimer() {
        mAutoDismissTimer = new Handler(ThreadUtils.getUiThreadLooper());
    }

    /** Reset the timer. Do nothing if this timer has been cancelled already. */
    public void resetTimer() {
        if (mRunnableOnTimeUp == null) return;
        Runnable runnable = mRunnableOnTimeUp;
        cancelTimer();
        startTimer(mDuration, runnable);
    }

    /** Cancel the timer. The registered runnable will not be run. */
    public void cancelTimer() {
        if (mRunnableOnTimeUp == null) return;
        mAutoDismissTimer.removeCallbacksAndMessages(null);
        mRunnableOnTimeUp = null;
    }

    /**
     * Starts the timer.
     *
     * <p>The provided {@link Runnable} will be executed after the specified duration. If a timer
     * was already running, it will be cancelled before the new one starts.
     *
     * @param duration The time (in milliseconds) to wait.
     * @param runnableOnTimeUp Executed when the timer expires.
     */
    public void startTimer(long duration, Runnable runnableOnTimeUp) {
        mDuration = duration;
        assert mDuration > 0;
        mRunnableOnTimeUp = runnableOnTimeUp;
        mAutoDismissTimer.postDelayed(mRunnableOnTimeUp, mDuration);
    }

    void setHandlerForTesting(Handler handler) {
        mAutoDismissTimer = handler;
    }

    @Nullable Runnable getRunnableOnTimeUpForTesting() {
        return mRunnableOnTimeUp;
    }
}
