// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.graphics.Insets;
import androidx.core.view.DisplayCutoutCompat;
import androidx.core.view.OnApplyWindowInsetsListener;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsAnimationCompat.BoundsCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.ui.base.ImmutableWeakReference;

import java.util.ArrayList;
import java.util.List;

/**
 * The purpose of this class is to store the system window insets (OSK, status bar) for
 * later use.
 */
public class InsetObserver implements OnApplyWindowInsetsListener {
    private final Rect mWindowInsets;
    private final Rect mCurrentSafeArea;
    private int mKeyboardInset;
    protected final ObserverList<WindowInsetObserver> mObservers;
    private final KeyboardInsetObservableSupplier mKeyboardInsetSupplier;
    private final WindowInsetsAnimationCompat.Callback mWindowInsetsAnimationProxyCallback;
    private final ObserverList<WindowInsetsAnimationListener> mWindowInsetsAnimationListeners =
            new ObserverList<>();
    private final List<WindowInsetsConsumer> mInsetsConsumers = new ArrayList<>();
    private final ImmutableWeakReference<View> mRootViewReference;
    // Insets to be added to the current safe area.
    private int mBottomInsetsForEdgeToEdge;
    private final Rect mDisplayCutoutRect;

    // Cached state
    private WindowInsetsCompat mLastSeenRawWindowInset;
    private static @Nullable WindowInsetsCompat sInitialRawWindowInsetsForTesting;

    /** Allows observing changes to the window insets from Android system UI. */
    public interface WindowInsetObserver {
        /**
         * Triggered when the window insets have changed.
         *
         * @param left The left inset.
         * @param top The top inset.
         * @param right The right inset (but it feels so wrong).
         * @param bottom The bottom inset.
         */
        default void onInsetChanged(int left, int top, int right, int bottom) {}

        default void onKeyboardInsetChanged(int inset) {}

        /** Called when a new Display Cutout safe area is applied. */
        default void onSafeAreaChanged(Rect area) {}
    }

    /**
     * Alias for {@link  androidx.core.view.OnApplyWindowInsetsListener} to emphasize that an
     * implementing class expects to consume insets, not just observe them. "Consuming" means "my
     * view/component will adjust its size to account for the space required by the inset." For
     * instance, the omnibox could "consume" the IME (keyboard) inset by adjusting the height of its
     * list of suggestions. By default the android framework handles which views consume insets by
     * applying them down the view hierarchy. You only need to add a consumer here if you want to
     * enable custom behavior, e.g. you want to shield a specific view from inset changes by
     * consuming them elsewhere.
     */
    public interface WindowInsetsConsumer extends androidx.core.view.OnApplyWindowInsetsListener {}

    /**
     * Interface equivalent of {@link  WindowInsetsAnimationCompat.Callback}. This allows
     * implementers to be notified of inset animation progress, enabling synchronization of browser
     * UI changes with system inset changes. This synchronization is potentially imperfect on API
     * level <30. Note that the interface version currently disallows modification of the insets
     * dispatched to the subtree. See {@link WindowInsetsAnimationCompat.Callback} for more.
     */
    public interface WindowInsetsAnimationListener {
        void onPrepare(@NonNull WindowInsetsAnimationCompat animation);

        void onStart(
                @NonNull WindowInsetsAnimationCompat animation,
                @NonNull WindowInsetsAnimationCompat.BoundsCompat bounds);

        void onProgress(
                @NonNull WindowInsetsCompat windowInsetsCompat,
                @NonNull List<WindowInsetsAnimationCompat> list);

        void onEnd(@NonNull WindowInsetsAnimationCompat animation);
    }

    private static class KeyboardInsetObservableSupplier extends ObservableSupplierImpl<Integer>
            implements WindowInsetObserver {
        @Override
        public void onKeyboardInsetChanged(int inset) {
            this.set(inset);
        }
    }

    /**
     * Creates an instance of {@link InsetObserver}.
     *
     * @param rootViewWeakRef A weak reference to the root view of the app.
     */
    public InsetObserver(ImmutableWeakReference<View> rootViewWeakRef) {
        mRootViewReference = rootViewWeakRef;
        mWindowInsets = new Rect();
        mCurrentSafeArea = new Rect();
        mDisplayCutoutRect = new Rect();
        mKeyboardInset = 0;
        mObservers = new ObserverList<>();
        mKeyboardInsetSupplier = new KeyboardInsetObservableSupplier();
        addObserver(mKeyboardInsetSupplier);
        mWindowInsetsAnimationProxyCallback =
                new WindowInsetsAnimationCompat.Callback(
                        WindowInsetsAnimationCompat.Callback.DISPATCH_MODE_STOP) {
                    @Override
                    public void onPrepare(@NonNull WindowInsetsAnimationCompat animation) {
                        for (WindowInsetsAnimationListener listener :
                                mWindowInsetsAnimationListeners) {
                            listener.onPrepare(animation);
                        }
                        super.onPrepare(animation);
                    }

                    @NonNull
                    @Override
                    public BoundsCompat onStart(
                            @NonNull WindowInsetsAnimationCompat animation,
                            @NonNull BoundsCompat bounds) {
                        for (WindowInsetsAnimationListener listener :
                                mWindowInsetsAnimationListeners) {
                            listener.onStart(animation, bounds);
                        }
                        return super.onStart(animation, bounds);
                    }

                    @Override
                    public void onEnd(@NonNull WindowInsetsAnimationCompat animation) {
                        for (WindowInsetsAnimationListener listener :
                                mWindowInsetsAnimationListeners) {
                            listener.onEnd(animation);
                        }
                        super.onEnd(animation);
                    }

                    @NonNull
                    @Override
                    public WindowInsetsCompat onProgress(
                            @NonNull WindowInsetsCompat windowInsetsCompat,
                            @NonNull List<WindowInsetsAnimationCompat> list) {
                        for (WindowInsetsAnimationListener listener :
                                mWindowInsetsAnimationListeners) {
                            listener.onProgress(windowInsetsCompat, list);
                        }
                        return windowInsetsCompat;
                    }
                };

        View rootView = getRootView();
        if (rootView == null) return;

        // Populate the root window insets if available.
        if (rootView.getRootWindowInsets() != null) {
            mLastSeenRawWindowInset =
                    WindowInsetsCompat.toWindowInsetsCompat(rootView.getRootWindowInsets());
        } else if (sInitialRawWindowInsetsForTesting != null) {
            mLastSeenRawWindowInset = sInitialRawWindowInsetsForTesting;
        }
        ViewCompat.setWindowInsetsAnimationCallback(rootView, mWindowInsetsAnimationProxyCallback);
        ViewCompat.setOnApplyWindowInsetsListener(rootView, this);
    }

    /**
     * Returns a supplier that observes this {@link InsetObserver} and
     * provides changes to the keyboard inset using the {@link
     * ObservableSupplier} interface.
     */
    public ObservableSupplier<Integer> getSupplierForKeyboardInset() {
        return mKeyboardInsetSupplier;
    }

    /**
     * Add a consumer of window insets. Consumers are given the opportunity to consume insets in
     * the order they're added.
     */
    public void addInsetsConsumer(@NonNull WindowInsetsConsumer insetConsumer) {
        mInsetsConsumers.add(insetConsumer);
    }

    /** Remove a consumer of window insets.*/
    public void removeInsetsConsumer(@NonNull WindowInsetsConsumer insetConsumer) {
        mInsetsConsumers.remove(insetConsumer);
    }

    /** Add a listener for inset animations. */
    public void addWindowInsetsAnimationListener(@NonNull WindowInsetsAnimationListener listener) {
        mWindowInsetsAnimationListeners.addObserver(listener);
    }

    /** Remove a listener for inset animations. */
    public void removeWindowInsetsAnimationListener(
            @NonNull WindowInsetsAnimationListener listener) {
        mWindowInsetsAnimationListeners.removeObserver(listener);
    }

    /** Add an observer to be notified when the window insets have changed. */
    public void addObserver(WindowInsetObserver observer) {
        mObservers.addObserver(observer);
    }

    /** Remove an observer of window inset changes. */
    public void removeObserver(WindowInsetObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Return the last seen raw window insets from the system. Insets will be returned as original,
     * so modifying this WindowInsets (e.g. by {@link WindowInsetsCompat#inset} is not recommended.
     * This should only be used for clients interested in reading a specific type of the insets;
     * otherwise, the client should be registered as a {@link WindowInsetsConsumer}.
     */
    @Nullable
    public WindowInsetsCompat getLastRawWindowInsets() {
        return mLastSeenRawWindowInset;
    }

    /**
     * Return the current safe area rect tracked by this InsetObserver. The Rect will be returned as
     * original being cached with pixel sizes, so it's not recommended to modify this rect.
     */
    public Rect getCurrentSafeArea() {
        return mCurrentSafeArea;
    }

    public WindowInsetsAnimationCompat.Callback getInsetAnimationProxyCallbackForTesting() {
        return mWindowInsetsAnimationProxyCallback;
    }

    @NonNull
    @Override
    public WindowInsetsCompat onApplyWindowInsets(
            @NonNull View view, @NonNull WindowInsetsCompat insets) {
        mLastSeenRawWindowInset = insets;

        updateDisplayCutoutRect(insets);
        insets = forwardToInsetConsumers(insets);
        updateKeyboardInset();

        Insets systemInsets = insets.getInsets(WindowInsetsCompat.Type.systemBars());
        onInsetChanged(
                systemInsets.left, systemInsets.top, systemInsets.right, systemInsets.bottom);
        insets =
                WindowInsetsCompat.toWindowInsetsCompat(
                        view.onApplyWindowInsets(insets.toWindowInsets()));
        return insets;
    }

    /**
     * Updates the window insets and notifies all observers if the values did indeed change.
     *
     * @param left The updated left inset.
     * @param top The updated right inset.
     * @param right The updated right inset.
     * @param bottom The updated bottom inset.
     */
    private void onInsetChanged(int left, int top, int right, int bottom) {
        if (mWindowInsets.left == left
                && mWindowInsets.top == top
                && mWindowInsets.right == right
                && mWindowInsets.bottom == bottom) {
            return;
        }

        mWindowInsets.set(left, top, right, bottom);

        for (WindowInsetObserver observer : mObservers) {
            observer.onInsetChanged(left, top, right, bottom);
        }
    }

    private void updateKeyboardInset() {
        View rootView = mRootViewReference.get();
        if (rootView == null) return;

        int keyboardInset = KeyboardUtils.calculateKeyboardHeightFromWindowInsets(rootView);

        if (mKeyboardInset == keyboardInset) {
            return;
        }

        mKeyboardInset = keyboardInset;

        for (WindowInsetObserver mObserver : mObservers) {
            mObserver.onKeyboardInsetChanged(mKeyboardInset);
        }
    }

    private WindowInsetsCompat forwardToInsetConsumers(WindowInsetsCompat insets) {
        View rootView = mRootViewReference.get();
        if (rootView == null) return insets;

        for (WindowInsetsConsumer consumer : mInsetsConsumers) {
            insets = consumer.onApplyWindowInsets(rootView, insets);
        }
        return insets;
    }

    /** Get the safe area from the WindowInsets, store it and notify any observers. */
    private void updateCurrentSafeArea() {
        Rect newSafeArea = new Rect(mDisplayCutoutRect);
        newSafeArea.bottom += mBottomInsetsForEdgeToEdge;
        // If the safe area has not changed then we should stop now.
        if (newSafeArea.equals(mCurrentSafeArea)) {
            return;
        }

        mCurrentSafeArea.set(newSafeArea);
        // Create a new rect to avoid rect being changed by observers.
        for (WindowInsetObserver mObserver : mObservers) {
            mObserver.onSafeAreaChanged(new Rect(mCurrentSafeArea));
        }
    }

    private void updateDisplayCutoutRect(final WindowInsetsCompat insets) {
        DisplayCutoutCompat displayCutout = insets.getDisplayCutout();
        Rect rect = new Rect();
        if (displayCutout != null) {
            rect.set(
                    displayCutout.getSafeInsetLeft(),
                    displayCutout.getSafeInsetTop(),
                    displayCutout.getSafeInsetRight(),
                    displayCutout.getSafeInsetBottom());
        }
        mDisplayCutoutRect.set(rect);
        updateCurrentSafeArea();
    }

    /**
     * @param bottomInset The bottom system insets tracked in edge to edge.
     */
    public void updateBottomInsetForEdgeToEdge(int bottomInset) {
        // When updating toEdge with bottom insets, meaning we should update the safe area
        // accordingly.
        if (mBottomInsetsForEdgeToEdge == bottomInset) return;

        mBottomInsetsForEdgeToEdge = bottomInset;
        updateCurrentSafeArea();
    }

    /**
     * Sets the initial raw window insets for a testing environment. Note - if using mocks, please
     * mock the #getInsets() method to return some valid insets.
     */
    public static void setInitialRawWindowInsetsForTesting(WindowInsetsCompat windowInsets) {
        sInitialRawWindowInsetsForTesting = windowInsets;
        ResettersForTesting.register(() -> sInitialRawWindowInsetsForTesting = null);
    }

    private View getRootView() {
        return mRootViewReference.get();
    }
}
