// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ProgressBar;

import org.chromium.base.ResettersForTesting;
import org.chromium.ui.interpolators.Interpolators;

import java.util.ArrayList;
import java.util.List;

/** A {@link ProgressBar} that understands the hiding/showing policy defined in Material Design. */
public class LoadingView extends ProgressBar {
    private static final int LOADING_ANIMATION_DELAY_MS = 500;
    private static final int MINIMUM_ANIMATION_SHOW_TIME_MS = 500;

    /** A observer interface that will be notified when the progress bar is hidden. */
    public interface Observer {
        /**
         * Notify the listener a call to {@link #showLoadingUI()} is complete and loading view
         * is VISIBLE.
         */
        void onShowLoadingUIComplete();

        /**
         * Notify the listener a call to {@link #hideLoadingUI()} is complete and loading view is
         * GONE.
         */
        void onHideLoadingUIComplete();
    }

    private long mStartTime = -1;
    private static boolean sDisableAnimationForTest;

    private final List<Observer> mObservers = new ArrayList<>();

    private final Runnable mDelayedShow =
            new Runnable() {
                @Override
                public void run() {
                    if (!mShouldShow) return;
                    mStartTime = SystemClock.elapsedRealtime();
                    setVisibility(View.VISIBLE);
                    setAlpha(1.0f);

                    for (Observer observer : mObservers) {
                        observer.onShowLoadingUIComplete();
                    }
                }
            };

    /**
     * Tracks whether the View should be displayed when {@link #mDelayedShow} is run.  Android
     * doesn't always cancel a Runnable when requested, meaning that the View could be hidden before
     * it even has a chance to be shown.
     */
    private boolean mShouldShow;

    // Material loading design spec requires us to show progress spinner at least 500ms, so we need
    // this delayed runnable to implement that.
    private final Runnable mDelayedHide =
            new Runnable() {
                @Override
                public void run() {
                    if (sDisableAnimationForTest) {
                        onHideLoadingFinished();
                        return;
                    }

                    animate()
                            .alpha(0.0f)
                            .setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR)
                            .setListener(
                                    new AnimatorListenerAdapter() {
                                        @Override
                                        public void onAnimationEnd(Animator animation) {
                                            onHideLoadingFinished();
                                        }
                                    });
                }
            };

    /** Constructor for creating the view programmatically. */
    public LoadingView(Context context) {
        super(context);
    }

    /** Constructor for inflating from XML. */
    public LoadingView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /** Shows loading UI with a delay by calling showLoadingUI(false). */
    public void showLoadingUI() {
        showLoadingUI(/* skipDelay= */ false);
    }

    /**
     * Show the loading UI. If skipDelay is set to true, the delay before the loading animation will
     * be skipped. If skipDelay is set to false, the loading animation will be shown after a delay
     * based on LOADING_ANIMATION_DELAY_MS (500ms).
     */
    public void showLoadingUI(boolean skipDelay) {
        removeCallbacks(mDelayedShow);
        removeCallbacks(mDelayedHide);
        mShouldShow = true;

        setVisibility(GONE);

        if (skipDelay) {
            mDelayedShow.run();
        } else {
            postDelayed(mDelayedShow, LOADING_ANIMATION_DELAY_MS);
        }
    }

    /**
     * Hide loading UI. If progress bar is not shown, it disappears immediately. If so, it smoothly
     * fades out.
     */
    public void hideLoadingUI() {
        removeCallbacks(mDelayedShow);
        removeCallbacks(mDelayedHide);
        mShouldShow = false;

        if (getVisibility() == VISIBLE) {
            postDelayed(
                    mDelayedHide,
                    Math.max(
                            0,
                            mStartTime
                                    + MINIMUM_ANIMATION_SHOW_TIME_MS
                                    - SystemClock.elapsedRealtime()));
        } else {
            onHideLoadingFinished();
        }
    }

    /** Remove all callbacks when this view is no longer needed. */
    public void destroy() {
        removeCallbacks(mDelayedShow);
        removeCallbacks(mDelayedHide);
        mObservers.clear();
    }

    /**
     * Add the listener that will be notified when the spinner is completely hidden with {@link
     * #hideLoadingUI()}.
     * @param listener {@link Observer} that will be notified when the spinner is
     *         completely hidden with {@link #hideLoadingUI()}.
     */
    public void addObserver(Observer listener) {
        mObservers.add(listener);
    }

    private void onHideLoadingFinished() {
        setVisibility(GONE);
        for (Observer observer : mObservers) {
            observer.onHideLoadingUIComplete();
        }
    }

    /**
     * Set disable the fading animation during {@link #hideLoadingUI()}.
     * This function is added as a work around for disable animation during unit tests.
     * @param disableAnimation Whether the fading animation should be disabled during {@link
     *         #hideLoadingUI()}.
     */
    public static void setDisableAnimationForTest(boolean disableAnimation) {
        sDisableAnimationForTest = disableAnimation;
        ResettersForTesting.register(() -> sDisableAnimationForTest = false);
    }

    /**
     * Check if the Loading View Observer is empty or not.
     * @return If the observers is empty then return true.
     */
    public boolean isObserverListEmpty() {
        return mObservers.isEmpty();
    }
}
