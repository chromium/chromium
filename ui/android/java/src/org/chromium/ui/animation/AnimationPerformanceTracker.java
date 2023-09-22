// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import android.os.SystemClock;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;

/**
 * Helper class for monitoring animation performance. Each {@link AnimationPerformanceTracker} can
 * track a single animation at a time and can supply results to multiple {@link Listeners}.
 */
public class AnimationPerformanceTracker {
    /** Tracks metrics about animation performance. */
    public static class AnimationMetrics {
        private long mStartTimeMs;
        private long mLastFrameTimeMs;
        private long mFirstFrameTimeMs;
        private long mFirstFrameLatencyMs;
        private long mMaxFrameIntervalMs;
        private int mFrameCount;
        private long mElapsedTimeMs;

        /** Returns the start time in milliseconds of the animation. */
        public long getStartTimeMs() {
            return mStartTimeMs;
        }

        /** Returns the time in milliseconds when the first frame of the animation occurred. */
        public long getFirstFrameTimeMs() {
            return mFirstFrameTimeMs;
        }

        /**
         * Returns the time in milliseconds between the animation start and the first frame.
         * This is a cached version of {@code getFirstFrameTimeMs() - getStartTimeMs()}.
         */
        public long getFirstFrameLatencyMs() {
            return mFirstFrameLatencyMs;
        }

        /** Returns the time of the last frame of the animation in milliseconds. */
        public long getLastFrameTimeMs() {
            return mLastFrameTimeMs;
        }

        /** Returns the maximum time interval between frames in milliseconds. */
        public long getMaxFrameIntervalMs() {
            return mMaxFrameIntervalMs;
        }

        /** Returns the total number of frames shown. */
        public long getFrameCount() {
            return mFrameCount;
        }

        /**
         * Returns the elapsed time in milliseconds the animation took to complete. This is the
         * time delta between construction and {@link #onEnd()}.
         */
        public long getElapsedTimeMs() {
            return mElapsedTimeMs;
        }

        /** Returns the number of frames per second for the animation. */
        public float getFramesPerSecond() {
            return 1000.f * mFrameCount / mElapsedTimeMs;
        }

        private AnimationMetrics() {
            mStartTimeMs = SystemClock.elapsedRealtime();
            mLastFrameTimeMs = -1L;
            mFirstFrameTimeMs = -1L;
            mFirstFrameLatencyMs = -1L;
            mMaxFrameIntervalMs = -1L;
            mElapsedTimeMs = -1L;
        }

        private void onUpdate() {
            final long currentTimeMs = SystemClock.elapsedRealtime();
            if (mFrameCount == 0) {
                mMaxFrameIntervalMs = currentTimeMs - mStartTimeMs;
                mFirstFrameLatencyMs = mMaxFrameIntervalMs;
                mFirstFrameTimeMs = currentTimeMs;
            } else {
                mMaxFrameIntervalMs =
                        Math.max(mMaxFrameIntervalMs, currentTimeMs - mLastFrameTimeMs);
            }
            mLastFrameTimeMs = currentTimeMs;
            mFrameCount++;
        }

        private void onEnd() {
            mElapsedTimeMs = SystemClock.elapsedRealtime() - mStartTimeMs;
        }
    }

    /** Listener to receive and process {@link AnimationMetrics} when the animation ends. */
    @FunctionalInterface
    public interface Listener {
        /**
         * Called when the animation ends.
         * @param metrics The {@link AnimationMetrics} for the completed animation.
         */
        public void onAnimationEnd(AnimationMetrics metrics);
    }

    private final ObserverList<Listener> mListeners = new ObserverList<>();
    private @Nullable AnimationMetrics mCurrentAnimationMetrics;

    /** Adds a {@link Listener} to be notified when an animation ends. */
    public void addListener(Listener listener) {
        mListeners.addObserver(listener);
    }

    /** Removes a {@link Listener} that was previously added in {@link #addListener(Listener)}. */
    public void removeListener(Listener listener) {
        mListeners.removeObserver(listener);
    }

    /**
     * Should be called when the animation is started. Can be called again after {@link #onEnd()} in
     * the case the tracker will be reused or the animation is repeatable.
     */
    public void onStart() {
        assert mCurrentAnimationMetrics == null : "Current animation has not finished.";
        mCurrentAnimationMetrics = new AnimationMetrics();
    }

    /** Should be called once per animation frame after {@link onStart()}. */
    public void onUpdate() {
        assert mCurrentAnimationMetrics != null : "No animation was started.";
        if (mCurrentAnimationMetrics == null) return;
        mCurrentAnimationMetrics.onUpdate();
    }

    /**
     * Should be called once when the animation finishes after {@link #onStart()} and any number of
     * {@link #onUpdate()}s.
     */
    public void onEnd() {
        assert mCurrentAnimationMetrics != null : "No animation was started.";
        if (mCurrentAnimationMetrics == null) return;
        mCurrentAnimationMetrics.onEnd();
        for (Listener listener : mListeners) {
            listener.onAnimationEnd(mCurrentAnimationMetrics);
        }
        mCurrentAnimationMetrics = null;
    }
}
