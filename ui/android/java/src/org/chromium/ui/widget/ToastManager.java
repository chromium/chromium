// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.os.Build;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;

import java.util.Iterator;
import java.util.PriorityQueue;

/**
 * Manages Android toasts based on their priorities.
 * <ul>
 * <li>Queues the requested toasts and shows them one by one in the order of the requested
 *     time if they have the same priority.</li>
 * <li>Shows the toast with high priority ahead of other queued normal priority ones.</li>
 * <li>Does not show the requested one again if it is already in the queue or currently
 *     showing. Toasts of same text content are regarded as duplicated.</li>
 * </ul>
 */
@JNINamespace("ui")
public class ToastManager {
    private static final int DURATION_SHORT_MS = 2000;
    private static final int DURATION_LONG_MS = 3500;

    private static ToastManager sInstance;

    // A queue for toasts waiting to be shown.
    private final PriorityQueue<Toast> mToastQueue =
            new PriorityQueue<>((toast1, toast2) -> toast1.getPriority() - toast2.getPriority());

    // Handles toast events per SDK version.
    private interface ToastEvent {
        void onShow(Toast toast);

        void onCancel();
    }

    private final ToastEvent mToastEvent;

    // Toast currently showing. {@code null} if none is showing.
    private Toast mToast;

    static ToastManager getInstance() {
        if (sInstance == null) sInstance = new ToastManager();
        return sInstance;
    }

    private ToastManager() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            mToastEvent = new ToastEventPreR(this::showNextToast);
        } else {
            mToastEvent = new ToastEventR(this::showNextToast);
        }
    }

    /**
     * Request to show a toast.
     * @param toast {@link Toast} object to show.
     */
    public void requestShow(Toast toast) {
        if (toast == null || isDuplicatedToast(toast)) return;

        mToastQueue.add(toast);

        if (getCurrentToast() == null) showNextToast();
    }

    /**
     * Cancel a toast if it is showing now, or removes it from the queue if found in it.
     * @param toast {@link Toast} to cancel.
     */
    public void cancel(Toast toast) {
        if (toast == getCurrentToast()) {
            cancelAndShowNextToast();
        } else {
            Iterator it = mToastQueue.iterator();
            Toast toastToRemove = null;
            while (it.hasNext()) {
                Toast t = (Toast) it.next();
                if (TextUtils.equals(t.getText(), toast.getText())) {
                    toastToRemove = t;
                    break;
                }
            }
            if (toastToRemove != null) mToastQueue.remove(toastToRemove);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    Toast getCurrentToast() {
        return mToast;
    }

    /** Check if we already have the same Toast object showing on the screen or in the queue. */
    private boolean isDuplicatedToast(Toast toast) {
        assert toast != null;
        Toast ct = getCurrentToast();
        if (ct != null && (ct == toast || TextUtils.equals(ct.getText(), toast.getText()))) {
            return true;
        }

        Iterator it = mToastQueue.iterator();
        while (it.hasNext()) {
            Toast t = (Toast) it.next();
            if (t == toast || TextUtils.equals(t.getText(), toast.getText())) {
                return true;
            }
        }
        return false;
    }

    private void showNextToast() {
        mToast = mToastQueue.poll(); // Retrieves and removes head of the queue.
        if (mToast != null) {
            mToast.getAndroidToast().show();
            mToastEvent.onShow(mToast);
        }
    }

    private void cancelAndShowNextToast() {
        assert mToast != null : "Current toast cannot be null";
        mToast.getAndroidToast().cancel();
        mToast = null;
        mToastEvent.onCancel();
    }

    private class ToastEventPreR implements ToastEvent {
        private final Handler mHandler = new Handler();
        private final Runnable mPostToastRunnable;

        ToastEventPreR(Runnable finishRunnable) {
            mPostToastRunnable = finishRunnable;
        }

        @Override
        public void onShow(Toast toast) {
            int durationMs =
                    (mToast.getDuration() == Toast.LENGTH_SHORT)
                            ? DURATION_SHORT_MS
                            : DURATION_LONG_MS;
            mHandler.postDelayed(mPostToastRunnable, durationMs);
        }

        @Override
        public void onCancel() {
            mHandler.removeCallbacks(mPostToastRunnable);
            mPostToastRunnable.run();
        }
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private static class ToastEventR implements ToastEvent {
        private final android.widget.Toast.Callback mToastCallback;

        ToastEventR(Runnable finishRunnable) {
            mToastCallback =
                    new android.widget.Toast.Callback() {
                        @Override
                        public void onToastHidden() {
                            finishRunnable.run();
                        }
                    };
        }

        @Override
        public void onShow(Toast toast) {
            toast.getAndroidToast().addCallback(mToastCallback);
        }

        @Override
        public void onCancel() {
            // On R+, Callback#onToastHidden handles |showNextToast| when canceled.
        }
    }

    /**
     * Resets ToastManager state to initial state. Cancels the current toast if present,
     * and clears the queue. This prevernts a test running a toast from interfering another one.
     */
    public static void resetForTesting() {
        getInstance().resetInternalForTesting(); // IN-TEST
    }

    private void resetInternalForTesting() {
        mToastQueue.clear();
        if (mToast != null) cancel(mToast);
    }

    boolean isShowingForTesting() {
        return mToast != null;
    }
}
