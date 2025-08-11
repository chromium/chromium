// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Handler;
import android.os.StrictMode;
import android.provider.Settings;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;

import java.util.concurrent.atomic.AtomicInteger;

/** Utility methods used for Android's software keyboard. */
@NullMarked
public final class KeyboardUtils {
    private static final String TAG = "KeyboardVisibility";

    /** Number of retries after a failed attempt of bringing up the keyboard. */
    private static final int KEYBOARD_RETRY_ATTEMPTS = 10;

    /** Waiting time between attempts to show the keyboard. */
    private static final long KEYBOARD_RETRY_DELAY_MS = 100;

    public static final int CTRL = 1 << 31;
    public static final int ALT = 1 << 30;
    public static final int SHIFT = 1 << 29;
    public static final int NO_MODIFIER = 0;

    /**
     * Tries to show the soft keyboard by using the {@link Context#INPUT_METHOD_SERVICE}.
     *
     * @param view The currently focused {@link View}, which would receive soft keyboard input.
     */
    public static void showKeyboard(View view) {
        final Handler handler = new Handler();
        final AtomicInteger attempt = new AtomicInteger();
        Runnable openRunnable =
                new Runnable() {
                    @Override
                    public void run() {
                        // Not passing InputMethodManager.SHOW_IMPLICIT as it does not trigger the
                        // keyboard in landscape mode.
                        InputMethodManager imm =
                                (InputMethodManager)
                                        view.getContext()
                                                .getSystemService(Context.INPUT_METHOD_SERVICE);
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
     * Hides the soft keyboard by using the {@link Context#INPUT_METHOD_SERVICE}. This template
     * method simplifies mocking and the access to the soft keyboard in subclasses.
     *
     * @param view The {@link View} that is currently accepting input.
     * @return Whether the keyboard was visible before.
     */
    public static boolean hideAndroidSoftKeyboard(View view) {
        if (!view.isAttachedToWindow()) return false;
        InputMethodManager imm =
                (InputMethodManager)
                        view.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        return imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
    }

    /**
     * Calculates the keyboard height based on the bottom margin it causes for the given rootView.
     * It is used to determine whether the keyboard is visible.
     *
     * @param rootView A {@link View}.
     * @return The size of the bottom margin which most likely is exactly the keyboard size.
     */
    public static int calculateKeyboardHeightFromWindowInsets(View rootView) {
        try (TraceEvent te =
                TraceEvent.scoped("KeyboardVisibilityDelegate.calculateKeyboardHeight")) {
            if (rootView == null || rootView.getRootWindowInsets() == null) return 0;
            WindowInsetsCompat windowInsetsCompat =
                    WindowInsetsCompat.toWindowInsetsCompat(
                            rootView.getRootWindowInsets(), rootView);
            int imeHeightIncludingSystemBars =
                    windowInsetsCompat.getInsets(WindowInsetsCompat.Type.ime()).bottom;
            if (imeHeightIncludingSystemBars == 0) return 0;
            int bottomSystemBarsHeight =
                    windowInsetsCompat.getInsets(WindowInsetsCompat.Type.systemBars()).bottom;
            return imeHeightIncludingSystemBars - bottomSystemBarsHeight;
        }
    }

    /**
     * Detects whether or not the keyboard is showing. This is a best guess based on the height of
     * the keyboard as there is no standardized/foolproof way to do this. This template method
     * simplifies mocking and the access to the soft keyboard in subclasses.
     *
     * @param view A {@link View}.
     * @return Whether or not the software keyboard is visible.
     */
    public static boolean isAndroidSoftKeyboardShowing(View view) {
        View rootView = view.getRootView();
        return rootView != null && calculateKeyboardHeightFromWindowInsets(rootView) > 0;
    }

    /**
     * Detects whether the soft keyboard is expected to show when keyboard input is required.
     *
     * <p>When there is a physical keyboard and show_ime_with_hard_keyboard is off, the soft
     * keyboard is not shown.
     */
    public static boolean isSoftKeyboardEnabled(Context context) {
        return !isHardKeyboardConnected(context) || shouldShowImeWithHardwareKeyboard(context);
    }

    /** Detects whether there is a hardware keyboard connected. */
    public static boolean isHardKeyboardConnected(Context context) {
        return context.getResources().getConfiguration().keyboard == Configuration.KEYBOARD_QWERTY;
    }

    /**
     * Reports whether Software keyboard is requested to come up while Hardware keyboard is in use.
     * This helps with cases where the Hardware keyboard is unconventional (e.g. Yubikey), and the
     * User explicitly requests the System to show up the Software keyboard when interacting with
     * input fields. NOTE: This is the default behavior on emulated devices.
     */
    public static boolean shouldShowImeWithHardwareKeyboard(Context context) {
        return Settings.Secure.getInt(
                        context.getContentResolver(), "show_ime_with_hard_keyboard", 0)
                != 0;
    }

    /**
     * Reports the metaState of a KeyEvent, which gives information about the modifier keys that are
     * part of the event (e.g. Ctrl).
     *
     * @param event A {@link KeyEvent}.
     * @return Bitmask of the modifier keys included in the event.
     */
    public static int getMetaState(KeyEvent event) {
        return (event.isCtrlPressed() ? CTRL : 0)
                | (event.isAltPressed() ? ALT : 0)
                | (event.isShiftPressed() ? SHIFT : 0);
    }
}
