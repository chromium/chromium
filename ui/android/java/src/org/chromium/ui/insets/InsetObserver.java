// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.insets;

import android.graphics.Rect;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.DisplayCutoutCompat;
import androidx.core.view.OnApplyWindowInsetsListener;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsAnimationCompat.BoundsCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * The purpose of this class is to store the system window insets (OSK, status bar) for later use.
 */
@NullMarked
public class InsetObserver implements OnApplyWindowInsetsListener {
    private final Rect mWindowInsets;
    private final Rect mCurrentSafeArea;
    private int mKeyboardInset;
    private final Rect mSystemGestureInsets;
    protected final ObserverList<WindowInsetObserver> mObservers;
    private final KeyboardInsetObservableSupplier mKeyboardInsetSupplier;
    private final WindowInsetsAnimationCompat.Callback mWindowInsetsAnimationProxyCallback;
    private final ObserverList<WindowInsetsAnimationListener> mWindowInsetsAnimationListeners =
            new ObserverList<>();
    private final @Nullable WindowInsetsConsumer[] mInsetsConsumers =
            new WindowInsetsConsumer[InsetConsumerSource.COUNT];

    private final ImmutableWeakReference<View> mRootViewReference;
    // Insets to be added to the current safe area.
    private int mBottomInsetsForEdgeToEdge;
    private final Rect mDisplayCutoutRect;

    private final boolean mEnableKeyboardOverlayMode;
    // This is currently only being used by the DeferredImeWindowInsetApplicationCallback. If this
    // is to be used by other callers, it should be changed to some token system to ensure that
    // different callers don't interfere with each other.
    private boolean mIsKeyboardInOverlayMode;

    // Cached state
    private @Nullable WindowInsetsCompat mLastSeenRawWindowInset;
    private static @Nullable WindowInsetsCompat sInitialRawWindowInsetsForTesting;

    // Edge-to-edge investigations
    // True if non-zero navbar insets have ever been seen during the current session, false
    // otherwise.
    private boolean mHasSeenNonZeroNavBar;
    // Set to true after the first insets have been seen and verified.
    private boolean mHasSeenInsets;
    // Set to true when non-zero navbar insets have been seen for the first time to correct missing
    // navbar insets.
    private boolean mObservedNonZeroNavBarCorrection;
    // Set to true when navbar insets are missing for the first time after non-zero navbar insets
    // were seen, indicating a regression in navbar insets being provided.
    private boolean mObservedNonZeroNavBarRegression;

    /** Allows observing changes to the window insets from Android system UI. */
    public interface WindowInsetObserver {
        /**
         * Triggered when the window insets for the system bars have changed, after all consumers
         * has consumed the corresponding insets during {@link #onApplyWindowInsets}.
         */
        default void onInsetChanged() {}

        default void onSystemGestureInsetsChanged(int left, int top, int right, int bottom) {}

        /**
         * Called when the keyboard inset changes. Note that the keyboard inset passed to this
         * method does not take inset consumption into account so this value represents the raw IME
         * inset received from the system.
         *
         * @param inset The raw, non-consumed keyboard inset.
         */
        default void onKeyboardInsetChanged(int inset) {}

        /** Called when a new Display Cutout safe area is applied. */
        default void onSafeAreaChanged(Rect area) {}
    }

    /**
     * Alias for {@link androidx.core.view.OnApplyWindowInsetsListener} to emphasize that an
     * implementing class expects to consume insets, not just observe them. "Consuming" means "my
     * view/component will adjust its size to account for the space required by the inset." For
     * instance, the omnibox could "consume" the IME (keyboard) inset by adjusting the height of its
     * list of suggestions. By default the android framework handles which views consume insets by
     * applying them down the view hierarchy. You only need to add a consumer here if you want to
     * enable custom behavior, e.g. you want to shield a specific view from inset changes by
     * consuming them elsewhere.
     */
    public interface WindowInsetsConsumer extends androidx.core.view.OnApplyWindowInsetsListener {

        // Consumers will be given the opportunity to process applied insets based on the priority
        // defined here. A lower value of the consumer source means that insets will be forwarded to
        // this consumer before others with a higher value. Be cautious about impact on existing
        // consumers in case a reordering is required while adding a new consumer source.
        @IntDef({
            InsetConsumerSource.TEST_SOURCE,
            InsetConsumerSource.DEFERRED_IME_WINDOW_INSET_APPLICATION_CALLBACK,
            InsetConsumerSource.APP_HEADER_COORDINATOR_CAPTION,
            InsetConsumerSource.TOP_INSET_COORDINATOR,
            InsetConsumerSource.EDGE_TO_EDGE_CONTROLLER_CREATOR,
            InsetConsumerSource.EDGE_TO_EDGE_CONTROLLER_IMPL,
            InsetConsumerSource.EDGE_TO_EDGE_LAYOUT_COORDINATOR,
            InsetConsumerSource.APP_HEADER_COORDINATOR_BOTTOM,
            InsetConsumerSource.COUNT
        })
        @Retention(RetentionPolicy.SOURCE)
        @interface InsetConsumerSource {
            // For testing only.
            int TEST_SOURCE = 0;

            int DEFERRED_IME_WINDOW_INSET_APPLICATION_CALLBACK = 1;
            // The AppHeaderCoordinator should get highest priority to process and potentially
            // consume caption bar insets (and overlapping status bar insets) because this is
            // critical to drawing the tab strip in the caption bar area in a desktop window
            // correctly.
            int APP_HEADER_COORDINATOR_CAPTION = 2;

            // The TopInsetCoordinator should be placed before EDGE_TO_EDGE_LAYOUT_COORDINATOR since
            // it consumes the top padding (Status bar).
            int TOP_INSET_COORDINATOR = 3;

            // The EdgeToEdgeControllerCreator exists only to create the EdgeToEdgeControllerImpl
            // if the current configuration (including window insets) indicate that the current
            // devices supports the bottom chin feature. This creator will remove itself as an
            // inset consumer if it creates the EdgeToEdgeControllerImpl.
            int EDGE_TO_EDGE_CONTROLLER_CREATOR = 4;
            int EDGE_TO_EDGE_CONTROLLER_IMPL = 5;
            int EDGE_TO_EDGE_LAYOUT_COORDINATOR = 6;
            int APP_HEADER_COORDINATOR_BOTTOM = 7;

            // Update this whenever a consumer source is added or removed.
            int COUNT = 8;
        }
    }

    @IntDef({
        HasSeenNonZeroNavBarState.GESTURE_NAV_FIRST_NAVBAR_MISSING,
        HasSeenNonZeroNavBarState.GESTURE_NAV_CORRECTION,
        HasSeenNonZeroNavBarState.GESTURE_NAV_REGRESSED,
        HasSeenNonZeroNavBarState.TAPPABLE_NAV_FIRST_NAVBAR_MISSING,
        HasSeenNonZeroNavBarState.TAPPABLE_NAV_CORRECTION,
        HasSeenNonZeroNavBarState.TAPPABLE_NAV_REGRESSED,
        HasSeenNonZeroNavBarState.BOTH_NAV_FIRST_NAVBAR_MISSING,
        HasSeenNonZeroNavBarState.BOTH_NAV_CORRECTION,
        HasSeenNonZeroNavBarState.BOTH_NAV_REGRESSED,
        HasSeenNonZeroNavBarState.NEITHER_NAV_FIRST_NAVBAR_MISSING,
        HasSeenNonZeroNavBarState.NEITHER_NAV_CORRECTION,
        HasSeenNonZeroNavBarState.NEITHER_NAV_REGRESSED,
        HasSeenNonZeroNavBarState.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface HasSeenNonZeroNavBarState {
        int GESTURE_NAV_FIRST_NAVBAR_MISSING = 0;
        int GESTURE_NAV_CORRECTION = 1;
        int GESTURE_NAV_REGRESSED = 2;
        int TAPPABLE_NAV_FIRST_NAVBAR_MISSING = 3;
        int TAPPABLE_NAV_CORRECTION = 4;
        int TAPPABLE_NAV_REGRESSED = 5;
        int BOTH_NAV_FIRST_NAVBAR_MISSING = 6;
        int BOTH_NAV_CORRECTION = 7;
        int BOTH_NAV_REGRESSED = 8;
        int NEITHER_NAV_FIRST_NAVBAR_MISSING = 9;
        int NEITHER_NAV_CORRECTION = 10;
        int NEITHER_NAV_REGRESSED = 11;
        int COUNT = 12;
    }

    /**
     * Interface equivalent of {@link WindowInsetsAnimationCompat.Callback}. This allows
     * implementers to be notified of inset animation progress, enabling synchronization of browser
     * UI changes with system inset changes. This synchronization is potentially imperfect on API
     * level <30. Note that the interface version currently disallows modification of the insets
     * dispatched to the subtree. See {@link WindowInsetsAnimationCompat.Callback} for more.
     */
    public interface WindowInsetsAnimationListener {
        void onPrepare(WindowInsetsAnimationCompat animation);

        void onStart(
                WindowInsetsAnimationCompat animation,
                WindowInsetsAnimationCompat.BoundsCompat bounds);

        void onProgress(
                WindowInsetsCompat windowInsetsCompat, List<WindowInsetsAnimationCompat> list);

        void onEnd(WindowInsetsAnimationCompat animation);
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
     * @param enableKeyboardOverlayMode Whether the keyboard can be considered to be in "overlay"
     *     mode, where its inset shouldn't affect the size of the viewport.
     */
    public InsetObserver(
            ImmutableWeakReference<View> rootViewWeakRef, boolean enableKeyboardOverlayMode) {
        mRootViewReference = rootViewWeakRef;
        mEnableKeyboardOverlayMode = enableKeyboardOverlayMode;
        mWindowInsets = new Rect();
        mCurrentSafeArea = new Rect();
        mDisplayCutoutRect = new Rect();
        mKeyboardInset = 0;
        mSystemGestureInsets = new Rect();
        mObservers = new ObserverList<>();
        mKeyboardInsetSupplier = new KeyboardInsetObservableSupplier();
        mKeyboardInsetSupplier.set(mKeyboardInset);
        addObserver(mKeyboardInsetSupplier);
        mWindowInsetsAnimationProxyCallback =
                new WindowInsetsAnimationCompat.Callback(
                        WindowInsetsAnimationCompat.Callback.DISPATCH_MODE_STOP) {
                    @Override
                    public void onPrepare(WindowInsetsAnimationCompat animation) {
                        for (WindowInsetsAnimationListener listener :
                                mWindowInsetsAnimationListeners) {
                            listener.onPrepare(animation);
                        }
                        super.onPrepare(animation);
                    }

                    @Override
                    public BoundsCompat onStart(
                            WindowInsetsAnimationCompat animation, BoundsCompat bounds) {
                        for (WindowInsetsAnimationListener listener :
                                mWindowInsetsAnimationListeners) {
                            listener.onStart(animation, bounds);
                        }
                        return super.onStart(animation, bounds);
                    }

                    @Override
                    public void onEnd(WindowInsetsAnimationCompat animation) {
                        for (WindowInsetsAnimationListener listener :
                                mWindowInsetsAnimationListeners) {
                            listener.onEnd(animation);
                        }
                        super.onEnd(animation);
                    }

                    @Override
                    public WindowInsetsCompat onProgress(
                            WindowInsetsCompat windowInsetsCompat,
                            List<WindowInsetsAnimationCompat> list) {
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
     * Returns a supplier that observes this {@link InsetObserver} and provides changes to the
     * keyboard inset using the {@link ObservableSupplier} interface.
     */
    public ObservableSupplier<Integer> getSupplierForKeyboardInset() {
        return mKeyboardInsetSupplier;
    }

    /**
     * Add a consumer of window insets. Consumers are given the opportunity to consume insets in the
     * order of a pre-defined priority value.
     */
    public void addInsetsConsumer(
            WindowInsetsConsumer insetConsumer, @InsetConsumerSource int source) {
        assert mInsetsConsumers[source] == null : "Inset consumer source has already been added.";
        mInsetsConsumers[source] = insetConsumer;
    }

    /** Returns whether this InsetObserver currently has an inset consumer from the given source. */
    public boolean hasInsetsConsumer(@InsetConsumerSource int source) {
        return mInsetsConsumers[source] != null;
    }

    /** Remove a consumer of window insets. */
    public void removeInsetsConsumer(WindowInsetsConsumer insetConsumer) {
        for (int i = 0; i < mInsetsConsumers.length; i++) {
            if (mInsetsConsumers[i] == insetConsumer) {
                mInsetsConsumers[i] = null;
                return;
            }
        }
    }

    /**
     * Call {@link #onApplyWindowInsets(View, WindowInsetsCompat)} with the last seen raw window
     * insets, if {@link #getLastRawWindowInsets()} is not null.
     *
     * <p>WARNING: This is used when an inset consumer is added / removed after the initial insets
     * are populated. The added / removed inset consumer may change the consumed inset for the
     * following consumer and observers.
     */
    public void retriggerOnApplyWindowInsets() {
        if (mLastSeenRawWindowInset == null || mRootViewReference.get() == null) return;
        onApplyWindowInsets(mRootViewReference.get(), mLastSeenRawWindowInset);
    }

    /** Add a listener for inset animations. */
    public void addWindowInsetsAnimationListener(WindowInsetsAnimationListener listener) {
        mWindowInsetsAnimationListeners.addObserver(listener);
    }

    /** Remove a listener for inset animations. */
    public void removeWindowInsetsAnimationListener(WindowInsetsAnimationListener listener) {
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
    public @Nullable WindowInsetsCompat getLastRawWindowInsets() {
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

    // Checks the insets for any irregularities that might interfere with edge-to-edge logic. If
    // such irregularities are detected, this will record (an) appropriate histogram(s).
    @VisibleForTesting
    void verifyInsetsForEdgeToEdge(WindowInsetsCompat insets) {
        Insets navigationBarInsets =
                insets.getInsetsIgnoringVisibility(WindowInsetsCompat.Type.navigationBars());
        boolean nonZeroNavBarInsets =
                navigationBarInsets.left > 0
                        || navigationBarInsets.bottom > 0
                        || navigationBarInsets.right > 0;

        boolean isFirstVerification = !mHasSeenInsets;
        boolean shouldRecordZeroNavBarCorrection = false;
        boolean shouldRecordZeroNavBarRegression = false;
        if (!isFirstVerification
                && !mHasSeenNonZeroNavBar
                && nonZeroNavBarInsets
                && !mObservedNonZeroNavBarCorrection) {
            // Non-zero navbar insets have been observed, indicating a correction.
            shouldRecordZeroNavBarCorrection = true;
            mObservedNonZeroNavBarCorrection = true;
        } else if (!isFirstVerification
                && mHasSeenNonZeroNavBar
                && !nonZeroNavBarInsets
                && !mObservedNonZeroNavBarRegression) {
            // Missing navbar insets have been observed after non-zero insets had previously
            // been seen, indicating a regression.
            shouldRecordZeroNavBarRegression = true;
            mObservedNonZeroNavBarRegression = true;
        }
        mHasSeenInsets = true;
        mHasSeenNonZeroNavBar |= nonZeroNavBarInsets;

        Insets mandatorySystemGesturesInsets =
                insets.getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        boolean mandatorySystemGestureImpliesNavBarInset =
                mandatorySystemGesturesInsets.left > 0
                        || mandatorySystemGesturesInsets.bottom > 0
                        || mandatorySystemGesturesInsets.right > 0;

        // Mandatory system gestures insets include insets covering (and sometimes exceeding) the
        // system bars. This inset is more consistent than the system bars - if it seems like the
        // mandatory system gestures insets implies the presence of a navigation bar, but the
        // navigation bar insets are missing (ignoring viz to account for situations like
        // fullscreen), then record a histogram.
        boolean hasInsetMismatch = mandatorySystemGestureImpliesNavBarInset && !nonZeroNavBarInsets;
        boolean shouldRecordMismatch =
                hasInsetMismatch && (isFirstVerification || shouldRecordZeroNavBarRegression);
        if (!shouldRecordZeroNavBarCorrection && !shouldRecordMismatch) return;

        Insets systemGesturesInsets = insets.getInsets(WindowInsetsCompat.Type.systemGestures());
        Insets nonMandatorySystemGestures =
                Insets.subtract(systemGesturesInsets, mandatorySystemGesturesInsets);
        // In gesture navigation mode, the left and right sides typically have insets for
        // swiping gestures, but these are not considered mandatory system gestures because apps
        // can specify gesture exclusion rects for these edges.
        boolean inGestureNavMode =
                nonMandatorySystemGestures.left > 0 || nonMandatorySystemGestures.right > 0;

        Insets tappableInsets = insets.getInsets(WindowInsetsCompat.Type.tappableElement());
        boolean inTappableNavMode =
                tappableInsets.left > 0 || tappableInsets.bottom > 0 || tappableInsets.right > 0;

        @HasSeenNonZeroNavBarState int hasSeenNonZeroNavBarState;
        if (inGestureNavMode) {
            if (inTappableNavMode) {
                if (isFirstVerification) {
                    hasSeenNonZeroNavBarState =
                            HasSeenNonZeroNavBarState.BOTH_NAV_FIRST_NAVBAR_MISSING;
                } else if (shouldRecordZeroNavBarCorrection) {
                    hasSeenNonZeroNavBarState = HasSeenNonZeroNavBarState.BOTH_NAV_CORRECTION;
                } else {
                    hasSeenNonZeroNavBarState = HasSeenNonZeroNavBarState.BOTH_NAV_REGRESSED;
                }
            } else {
                if (isFirstVerification) {
                    hasSeenNonZeroNavBarState =
                            HasSeenNonZeroNavBarState.GESTURE_NAV_FIRST_NAVBAR_MISSING;
                } else if (shouldRecordZeroNavBarCorrection) {
                    hasSeenNonZeroNavBarState = HasSeenNonZeroNavBarState.GESTURE_NAV_CORRECTION;
                } else {
                    hasSeenNonZeroNavBarState = HasSeenNonZeroNavBarState.GESTURE_NAV_REGRESSED;
                }
            }
        } else {
            if (inTappableNavMode) {
                if (isFirstVerification) {
                    hasSeenNonZeroNavBarState =
                            HasSeenNonZeroNavBarState.TAPPABLE_NAV_FIRST_NAVBAR_MISSING;
                } else if (shouldRecordZeroNavBarCorrection) {
                    hasSeenNonZeroNavBarState = HasSeenNonZeroNavBarState.TAPPABLE_NAV_CORRECTION;
                } else {
                    hasSeenNonZeroNavBarState = HasSeenNonZeroNavBarState.TAPPABLE_NAV_REGRESSED;
                }
            } else {
                if (isFirstVerification) {
                    hasSeenNonZeroNavBarState =
                            HasSeenNonZeroNavBarState.NEITHER_NAV_FIRST_NAVBAR_MISSING;
                } else if (shouldRecordZeroNavBarCorrection) {
                    hasSeenNonZeroNavBarState = HasSeenNonZeroNavBarState.NEITHER_NAV_CORRECTION;
                } else {
                    hasSeenNonZeroNavBarState = HasSeenNonZeroNavBarState.NEITHER_NAV_REGRESSED;
                }
            }
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Android.EdgeToEdge.NavigationBarMandatoryGesturesMismatch",
                hasSeenNonZeroNavBarState,
                HasSeenNonZeroNavBarState.COUNT);
    }

    /**
     * Returns whether the InsetObserver has observed non-zero navigation bar insets (ignoring
     * visibility).
     */
    public boolean hasSeenNonZeroNavigationBarInsets() {
        return mHasSeenNonZeroNavBar;
    }

    @Override
    public WindowInsetsCompat onApplyWindowInsets(View view, WindowInsetsCompat insets) {
        mLastSeenRawWindowInset = insets;
        verifyInsetsForEdgeToEdge(insets);

        updateDisplayCutoutRect(insets);
        insets = forwardToInsetConsumers(insets);
        updateKeyboardInset();

        Insets systemInsets = insets.getInsets(WindowInsetsCompat.Type.systemBars());
        onInsetChanged(
                systemInsets.left, systemInsets.top, systemInsets.right, systemInsets.bottom);
        Insets systemGestureInsets = insets.getInsets(WindowInsetsCompat.Type.systemGestures());
        onSystemGestureInsetsChanged(
                systemGestureInsets.left,
                systemGestureInsets.top,
                systemGestureInsets.right,
                systemGestureInsets.bottom);
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
        if (mWindowInsets.left != left
                || mWindowInsets.top != top
                || mWindowInsets.right != right
                || mWindowInsets.bottom != bottom) {
            mWindowInsets.set(left, top, right, bottom);
        }

        for (WindowInsetObserver observer : mObservers) {
            observer.onInsetChanged();
        }
    }

    private void onSystemGestureInsetsChanged(int left, int top, int right, int bottom) {
        if (mSystemGestureInsets.left == left
                && mSystemGestureInsets.top == top
                && mSystemGestureInsets.right == right
                && mSystemGestureInsets.bottom == bottom) {
            return;
        }

        mSystemGestureInsets.set(left, top, right, bottom);

        for (WindowInsetObserver observer : mObservers) {
            observer.onSystemGestureInsetsChanged(left, top, right, bottom);
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
            if (consumer == null) continue;
            insets = consumer.onApplyWindowInsets(rootView, insets);
        }
        return insets;
    }

    /** Get the safe area from the WindowInsets, store it and notify any observers. */
    private void updateCurrentSafeArea() {
        // When display cutout already included in the system bar insets, do not consider it as safe
        // area.
        Insets systemBarInsets =
                getLastRawWindowInsets() == null
                        ? Insets.NONE
                        : getLastRawWindowInsets().getInsets(WindowInsetsCompat.Type.systemBars());
        Rect newSafeArea =
                new Rect(
                        Math.max(0, mDisplayCutoutRect.left - systemBarInsets.left),
                        Math.max(0, mDisplayCutoutRect.top - systemBarInsets.top),
                        Math.max(0, mDisplayCutoutRect.right - systemBarInsets.right),
                        Math.max(0, mDisplayCutoutRect.bottom - systemBarInsets.bottom));
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

    /**
     * Returns whether the keyboard is in overlay mode. When in overlay mode, the keyboard should
     * not resize the application view, and should be treated as a visual overlay as opposed to an
     * window inset that changes the size of the viewport.
     */
    public boolean isKeyboardInOverlayMode() {
        if (!mEnableKeyboardOverlayMode) return false;
        // Currently, the keyboard will only be in overlay mode specifically if the
        // DeferredIMEWindowInsetApplicationCallback is consuming the window IME insets.
        return mIsKeyboardInOverlayMode;
    }

    /**
     * Sets whether the keyboard should be in overlay mode.
     *
     * @param isKeyboardInOverlayMode Whether the keyboard should be in overlay mode.
     */
    public void setKeyboardInOverlayMode(boolean isKeyboardInOverlayMode) {
        mIsKeyboardInOverlayMode = isKeyboardInOverlayMode;
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

    private @Nullable View getRootView() {
        return mRootViewReference.get();
    }
}
