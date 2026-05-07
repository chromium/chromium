// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import org.chromium.base.ThreadUtils.ThreadChecker;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A wrapper around a successful execution {@link Runnable} that executes a fallback {@link
 * Runnable} after a specified duration if not run early. All operations including the timeout
 * runnable must be executed on the same thread.
 *
 * <p>If the timeout expires, the timeout runnable is executed instead of the primary one. If
 * cancelled, neither are executed. Only one of the two runnables will be executed, exactly once.
 *
 * <p>The timeout countdown must be explicitly started by calling {@link #startTimeout()}. It does
 * not start automatically upon construction.
 */
@NullMarked
public class TimeoutRunnable implements Runnable {
    private final ThreadChecker mThreadChecker = new ThreadChecker();
    private final long mDurationMs;

    private @Nullable Runnable mRunnable;
    private @Nullable Runnable mOnTimeout;
    private @Nullable TaskRunner mRunner;
    private boolean mTimeoutStarted;

    /**
     * Creates a runnable that times out if it is not called within |durationMs| after starting. It
     * cannot be called again after the timeout. When it times out, |onTimeout| is called instead.
     *
     * <p>The runnable will be run on the UI thread.
     *
     * @param runnable The runnable to run upon successful execution.
     * @param onTimeout The runnable to run when time expires.
     * @param durationMs The timeout duration in milliseconds.
     */
    public TimeoutRunnable(Runnable runnable, Runnable onTimeout, long durationMs) {
        this(runnable, onTimeout, durationMs, PostTask.getTaskRunner(TaskTraits.UI_DEFAULT));
    }

    /**
     * Creates a runnable that times out if it is not called within |durationMs| after starting. It
     * cannot be called again after the timeout. When it times out, |onTimeout| is called instead.
     *
     * @param runnable The runnable to run upon successful execution.
     * @param onTimeout The runnable to run when time expires.
     * @param durationMs The timeout duration in milliseconds.
     * @param runner The task runner to use for the timeout.
     */
    public TimeoutRunnable(
            Runnable runnable, Runnable onTimeout, long durationMs, TaskRunner runner) {
        mRunnable = runnable;
        mOnTimeout = onTimeout;
        mDurationMs = durationMs;
        mRunner = runner;
    }

    @Override
    public void run() {
        mThreadChecker.assertOnValidThread();
        if (mRunner == null) return;

        assertNotDestroyed();
        mRunnable.run();

        destroy();
    }

    /** Starts the timeout countdown. */
    public void startTimeout() {
        mThreadChecker.assertOnValidThread();
        if (mRunner == null || mTimeoutStarted) return;
        mTimeoutStarted = true;

        assertNotDestroyed();
        mRunner.postDelayedTask(this::expire, mDurationMs);
    }

    /** Cancels the runnable execution and cleans up internal references. */
    public void cancel() {
        mThreadChecker.assertOnValidThread();
        if (mRunner == null) return;

        assertNotDestroyed();
        destroy();
    }

    private void expire() {
        assert mTimeoutStarted;
        mThreadChecker.assertOnValidThread();
        if (mRunner == null) return;

        assertNotDestroyed();
        mOnTimeout.run();

        destroy();
    }

    private void destroy() {
        mRunnable = null;
        mOnTimeout = null;
        mRunner = null;
    }

    @EnsuresNonNull({"mRunner", "mRunnable", "mOnTimeout"})
    private void assertNotDestroyed() {
        assert mRunner != null;
        assert mRunnable != null;
        assert mOnTimeout != null;
    }
}
