// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.view.View;

import org.chromium.base.ObserverList;

/**
 * A delegate that can be overridden to change the methods to figure out and change the current
 * state of Android's soft keyboard.
 */
public class KeyboardVisibilityDelegate {

    /** The delegate to determine keyboard visibility. */
    private static KeyboardVisibilityDelegate sInstance = new KeyboardVisibilityDelegate();

    /** An interface to notify listeners of changes in the soft keyboard's visibility. */
    public interface KeyboardVisibilityListener {
        /**
         * Called whenever the keyboard might have changed.
         * @param isShowing A boolean that's true if the keyboard is now visible.
         */
        void keyboardVisibilityChanged(boolean isShowing);
    }

    private final ObserverList<KeyboardVisibilityListener> mKeyboardVisibilityListeners =
            new ObserverList<>();

    protected boolean hasKeyboardVisibilityListeners() {
        return !mKeyboardVisibilityListeners.isEmpty();
    }

    protected void registerKeyboardVisibilityCallbacks() {}

    protected void unregisterKeyboardVisibilityCallbacks() {}

    /**
     * Allows setting a new strategy to override the default {@link KeyboardVisibilityDelegate}.
     * Caution while using it as it will take precedence over the currently set strategy. If two
     * delegates are added, the newer one will try to handle any call. If it can't an older one is
     * called. New delegates can call |method| of a predecessor with {@code super.|method|}.
     *
     * @param delegate A {@link KeyboardVisibilityDelegate} instance.
     * @deprecated once {@link #getInstance()} is removed this is no longer required. See
     *     crbug.com/343936788.
     */
    @Deprecated
    public static void setInstance(KeyboardVisibilityDelegate delegate) {
        sInstance = delegate;
    }

    /**
     * Prefer using {@link org.chromium.ui.base.WindowAndroid#getKeyboardDelegate()} over this
     * method. Both return a delegate which allows checking and influencing the keyboard state.
     *
     * @return the global {@link KeyboardVisibilityDelegate}.
     * @deprecated get the instance from {@code WindowAndroid} instead. See crbug.com/343936788.
     */
    @Deprecated
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
     *
     * @param view The currently focused {@link View}, which would receive soft keyboard input.
     */
    public void showKeyboard(View view) {
        KeyboardUtils.showKeyboard(view);
    }

    /**
     * Hides the soft keyboard.
     *
     * @param view The {@link View} that is currently accepting input.
     * @return Whether the keyboard was visible before.
     */
    public boolean hideKeyboard(View view) {
        return KeyboardUtils.hideAndroidSoftKeyboard(view);
    }

    /**
     * Returns the total keyboard widget height.
     *
     * <p>In addition to the keyboard itself, this may include accessory bars and related widgets
     * that behave as-if they're part of the keyboard if the embedder supports them.
     *
     * @param rootView A {@link View}.
     * @return The the total keyboard widget height, including accessory bars if exists.
     */
    public int calculateTotalKeyboardHeight(View rootView) {
        return KeyboardUtils.calculateKeyboardHeightFromWindowInsets(rootView);
    }

    /**
     * Returns whether the keyboard is showing.
     *
     * @param context A {@link Context} instance.
     * @param view A {@link View}.
     * @return Whether or not the software keyboard is visible.
     */
    public boolean isKeyboardShowing(Context context, View view) {
        return KeyboardUtils.isAndroidSoftKeyboardShowing(view);
    }

    /**
     * To be called when the keyboard visibility state might have changed. Informs listeners of the
     * state change IFF there actually was a change.
     *
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
