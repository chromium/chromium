// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.os.Handler;
import android.os.StrictMode;
import android.view.View;
import android.view.WindowInsets;
import android.view.inputmethod.InputMethodManager;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * A delegate that can be overridden to change the methods to figure out and change the current
 * state of Android's soft keyboard.
 */
public class KeyboardVisibilityDelegate {
    private static final String TAG = "KeyboardVisibility";

    /** Number of retries after a failed attempt of bringing up the keyboard. */
    private static final int KEYBOARD_RETRY_ATTEMPTS = 10;

    /** Waiting time between attempts to show the keyboard. */
    private static final long KEYBOARD_RETRY_DELAY_MS = 100;

    /** The minimum size of the bottom margin below the app to detect a keyboard. */
    private static final float KEYBOARD_DETECT_BOTTOM_THRESHOLD_DP = 100;

    /** The delegate to determine keyboard visibility. */
    private static KeyboardVisibilityDelegate sInstance = new KeyboardVisibilityDelegate();

    /**
     * An interface to notify listeners of changes in the soft keyboard's visibility.
     */
    public interface KeyboardVisibilityListener {
        /**
         * Called whenever the keyboard might have changed.
         * @param isShowing A boolean that's true if the keyboard is now visible.
         */
        void keyboardVisibilityChanged(boolean isShowing);
    }
    private final ObserverList<KeyboardVisibilityListener> mKeyboardVisibilityListeners =
            new ObserverList<>();

    protected void registerKeyboardVisibilityCallbacks() {}
    protected void unregisterKeyboardVisibilityCallbacks() {}

    /**
     * Allows setting a new strategy to override the default {@link KeyboardVisibilityDelegate}.
     * Caution while using it as it will take precedence over the currently set strategy.
     * If two delegates are added, the newer one will try to handle any call. If it can't an older
     * one is called. New delegates can call |method| of a predecessor with {@code super.|method|}.
     * @param delegate A {@link KeyboardVisibilityDelegate} instance.
     */
    public static void setInstance(KeyboardVisibilityDelegate delegate) {
        sInstance = delegate;
    }

    /**
     * Prefer using {@link org.chromium.ui.base.WindowAndroid#getKeyboardDelegate()} over this
     * method. Both return a delegate which allows checking and influencing the keyboard state.
     * @return the global {@link KeyboardVisibilityDelegate}.
     */
    public static KeyboardVisibilityDelegate getInstance() {
        return sInstance;
    }

    /**
     * Only classes that override the delegate may instantiate it and set it using
     * {@link #setInstance(KeyboardVisibilityDelegate)}.
     */
    protected KeyboardVisibilityDelegate() {}

    /**
     * Tries to show the soft keyboard by using the {@link Context#INPUT_METHOD_SERVICE}.
     * @param view The currently focused {@link View}, which would receive soft keyboard input.
     */
    @SuppressLint("NewApi")
    public void showKeyboard(View view) {
        final Handler handler = new Handler();
        final AtomicInteger attempt = new AtomicInteger();
        Runnable openRunnable = new Runnable() {
            @Override
            public void run() {
                // Not passing InputMethodManager.SHOW_IMPLICIT as it does not trigger the
                // keyboard in landscape mode.
                InputMethodManager imm = (InputMethodManager) view.getContext().getSystemService(
                        Context.INPUT_METHOD_SERVICE);
                // Third-party touches disk on showSoftInput call.
                // http://crbug.com/619824, http://crbug.com/635118
                StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
                try {
                    imm.showSoftInput(view, 0);
                } catch (IllegalArgumentException e) {
                    if (attempt.incrementAndGet() <= KEYBOARD_RETRY_ATTEMPTS) {
                        handler.postDelayed(this, KEYBOARD_RETRY_DELAY_MS);
                    } else {
                        Log.e(TAG, "Unable to open keyboard.  Giving up.", e);
                    }
                } finally {
                    StrictMode.setThreadPolicy(oldPolicy);
                }
            }
        };
        openRunnable.run();
    }

    /**
     * Hides the soft keyboard.
     * @param view The {@link View} that is currently accepting input.
     * @return Whether the keyboard was visible before.
     */
    public boolean hideKeyboard(View view) {
        return hideAndroidSoftKeyboard(view);
    }

    /**
     * Hides the soft keyboard by using the {@link Context#INPUT_METHOD_SERVICE}.
     * This template method simplifies mocking and the access to the soft keyboard in subclasses.
     * @param view The {@link View} that is currently accepting input.
     * @return Whether the keyboard was visible before.
     */
    protected boolean hideAndroidSoftKeyboard(View view) {
        InputMethodManager imm = (InputMethodManager) view.getContext().getSystemService(
                Context.INPUT_METHOD_SERVICE);
        return imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
    }

    /**
     * Calculates the keyboard height based on the bottom margin it causes for the given
     * rootView. It is used to determine whether the keyboard is visible.
     * @param rootView A {@link View}.
     * @return The size of the bottom margin which most likely is exactly the keyboard size.
     */
    public int calculateKeyboardHeight(View rootView) {
        Rect appRect = new Rect();
        rootView.getWindowVisibleDisplayFrame(appRect);

        // Assume status bar is always at the top of the screen.
        final int statusBarHeight = appRect.top;

        int bottomMargin = rootView.getHeight() - (appRect.height() + statusBarHeight);

        // If there is no bottom margin, the keyboard is not showing.
        if (bottomMargin <= 0) return 0;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            WindowInsets insets = rootView.getRootWindowInsets();
            if (insets != null) { // Either not supported or the rootView isn't attached.
                bottomMargin -= insets.getStableInsetBottom();
            }
        }

        return bottomMargin; // This might include a bottom navigation.
    }

    protected int calculateKeyboardDetectionThreshold(Context context, View rootView) {
        Rect appRect = new Rect();
        rootView.getWindowVisibleDisplayFrame(appRect);

        // If the display frame width is < root view width, controls are on the side of
        // the screen. The inverse is not necessarily true; i.e. if navControlsOnSide is
        // false, it doesn't mean the controls are not on the side or that they _are_ at
        // the bottom. It might just mean the app is not responsible for drawing their
        // background.
        boolean navControlsOnSide = appRect.width() != rootView.getWidth();
        // If the Android nav controls are on the sides instead of at the bottom, its
        // height is not needed.
        if (navControlsOnSide) return 0;

        // Since M, window insets provide a good keyboard height - no guessing the nav required.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return 0;
        }
        // In the event we couldn't get the bottom nav height, use a best guess
        // of the keyboard height. In certain cases this also means including
        // the height of the Android navigation.
        final float density = context.getResources().getDisplayMetrics().density;
        return (int) (KEYBOARD_DETECT_BOTTOM_THRESHOLD_DP * density);
    }

    /**
     * Returns whether the keyboard is showing.
     * @param context A {@link Context} instance.
     * @param view    A {@link View}.
     * @return        Whether or not the software keyboard is visible.
     */
    public boolean isKeyboardShowing(Context context, View view) {
        return isAndroidSoftKeyboardShowing(context, view);
    }

    /**
     * Detects whether or not the keyboard is showing. This is a best guess based on the height
     * of the keyboard as there is no standardized/foolproof way to do this.
     * This template method simplifies mocking and the access to the soft keyboard in subclasses.
     * @param context A {@link Context} instance.
     * @param view    A {@link View}.
     * @return        Whether or not the software keyboard is visible.
     */
    protected boolean isAndroidSoftKeyboardShowing(Context context, View view) {
        View rootView = view.getRootView();
        return rootView != null
                && calculateKeyboardHeight(rootView)
                > calculateKeyboardDetectionThreshold(context, rootView);
    }

    /**
     * To be called when the keyboard visibility state might have changed. Informs listeners of the
     * state change IFF there actually was a change.
     * @param isShowing The current (guesstimated) state of the keyboard.
     */
    protected void notifyListeners(boolean isShowing) {
        for (KeyboardVisibilityListener listener : mKeyboardVisibilityListeners) {
            listener.keyboardVisibilityChanged(isShowing);
        }
    }

    /**
     * Adds a listener that is updated of keyboard visibility changes. This works as a best guess.
     *
     * @see org.chromium.ui.KeyboardVisibilityDelegate#isKeyboardShowing(Context, View)
     */
    public void addKeyboardVisibilityListener(KeyboardVisibilityListener listener) {
        if (mKeyboardVisibilityListeners.isEmpty()) {
            registerKeyboardVisibilityCallbacks();
        }
        mKeyboardVisibilityListeners.addObserver(listener);
    }

    /**
     * @see #addKeyboardVisibilityListener(KeyboardVisibilityListener)
     */
    public void removeKeyboardVisibilityListener(KeyboardVisibilityListener listener) {
        mKeyboardVisibilityListeners.removeObserver(listener);
        if (mKeyboardVisibilityListeners.isEmpty()) {
            unregisterKeyboardVisibilityCallbacks();
        }
    }
}
