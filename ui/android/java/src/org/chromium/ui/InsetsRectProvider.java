// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.graphics.Rect;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.util.Size;
import android.view.View;
import android.view.WindowInsets;

import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsCompat.Type.InsetsType;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.InsetObserver.WindowInsetsConsumer;
import org.chromium.ui.util.WindowInsetsUtils;

import java.util.List;

/**
 * Class that reads and consumes a specific type of {@link WindowInsets}, and determines the widest
 * unoccluded {@link Rect} within the insets region after taking the insets' bounding rects into
 * account. This class is intended to be used with {@link InsetObserver} attached to the root view
 * in the activity window, since it's expecting to read the size from the window frame.
 *
 * <p>A bounding rect is an area provided for a specific {@link WindowInsets}, usually representing
 * the area being occupied by the insets (e.g. display cutout, system UI). An unoccluded rect is an
 * area within the window insets region that is not covered by the bounding rects of that window
 * insets.
 *
 * <p>This class works only when the criteria is satisfied:
 * <li>1. Android version is at least R.
 * <li>2. WindowInsets of given type has insets from one side exactly.
 */
@NullMarked
public class InsetsRectProvider implements WindowInsetsConsumer {
    /** Observer interface that's interested in bounding rect updates. */
    public interface Observer {

        /** Notified when the bounding rect provided has an update. */
        void onBoundingRectsUpdated(Rect widestUnoccludedRect);
    }

    private final @InsetsType int mInsetType;
    private final Rect mWindowRect = new Rect();
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final InsetObserver mInsetObserver;

    private @Nullable WindowInsetsCompat mCachedInsets;
    private List<Rect> mBoundingRects;
    private Rect mWidestUnoccludedRect = new Rect();

    /**
     * Create a rect provider for a specific inset type. This class should only be used for Android
     * R+.
     *
     * @param insetObserver {@link InsetObserver} that's attached to the root view.
     * @param insetType {@link InsetsType} this provider is observing.
     * @param initialInsets The initial window insets that will be used to read the bounding rects.
     * @param insetConsumerSource The {@link InsetConsumerSource} of inset observation and
     *     consumption.
     */
    public InsetsRectProvider(
            InsetObserver insetObserver,
            @InsetsType int insetType,
            @Nullable WindowInsetsCompat initialInsets,
            @InsetConsumerSource int insetConsumerSource) {
        mInsetType = insetType;
        mBoundingRects = List.of();
        mInsetObserver = insetObserver;

        assert VERSION.SDK_INT >= VERSION_CODES.R;
        mInsetObserver.addInsetsConsumer(this, insetConsumerSource);
        if (initialInsets != null) {
            maybeUpdateWidestUnoccludedRect(initialInsets);
        }
    }

    /** Return the list of bounding rect from the window insets. */
    public List<Rect> getBoundingRects() {
        return mBoundingRects;
    }

    /**
     * Return the current widest unoccluded rect within the window insets region. An unoccluded rect
     * is an area within the window insets that is not covered by the bounding rects of that window
     * insets.
     */
    public Rect getWidestUnoccludedRect() {
        return mWidestUnoccludedRect;
    }

    /**
     * Return the last {@link Insets} seen by this instance, return an empty insets if no
     * WindowInsets is cached yet.
     */
    public Insets getCachedInset() {
        if (mCachedInsets == null) return Insets.of(new Rect());
        return mCachedInsets.getInsets(mInsetType);
    }

    /** Return the current window Rect. */
    public Rect getWindowRect() {
        return mWindowRect;
    }

    /** Add an observer for updates of bounding rects. */
    public void addObserver(Observer obs) {
        mObservers.addObserver(obs);
    }

    /** Remove an observer for updates of bounding rects. */
    public void removeObserver(Observer obs) {
        mObservers.removeObserver(obs);
    }

    /** Destroy the dependencies and clear the observers. */
    public void destroy() {
        mObservers.clear();
        mInsetObserver.removeInsetsConsumer(this);
    }

    // Implements WindowInsetsConsumer

    @Override
    public WindowInsetsCompat onApplyWindowInsets(
            View view, WindowInsetsCompat windowInsetsCompat) {
        // Ignore the input by version check.
        if (VERSION.SDK_INT < VERSION_CODES.R) {
            return windowInsetsCompat;
        }

        // Ignore the input if the insets were not used to adjust any view.
        if (!maybeUpdateWidestUnoccludedRect(windowInsetsCompat)) {
            return windowInsetsCompat;
        }

        // Consume the insets if used to adjust any view.
        return buildInsets(windowInsetsCompat, mInsetType, Insets.NONE);
    }

    /**
     * Gets the system bounding rects for the current inset type.
     *
     * @param windowInsetsCompat The current system {@link WindowInsetsCompat}.
     * @return The list of system bounding rects for the given inset type.
     */
    protected List<Rect> getBoundingRectsFromInsets(WindowInsetsCompat windowInsetsCompat) {
        return WindowInsetsUtils.getBoundingRectsFromInsets(
                windowInsetsCompat.toWindowInsets(), mInsetType);
    }

    /**
     * Build a new {@link WindowInsetsCompat} object. This is exposed to allow testing to override
     * this method, as the {@link WindowInsetsCompat.Builder} does not work in Robolectric tests.
     */
    @VisibleForTesting
    protected WindowInsetsCompat buildInsets(
            WindowInsetsCompat windowInsetsCompat, int insetType, Insets insets) {
        return new WindowInsetsCompat.Builder(windowInsetsCompat)
                .setInsets(insetType, insets)
                .build();
    }

    /**
     * @return Whether the applied window insets should be consumed by this class. {@code false}
     *     when the insets are not used to adjust any view, {@code true} otherwise. The insets
     *     should be consumed only if |mWidestUnoccludedRect| is non-empty to be customized.
     */
    private boolean maybeUpdateWidestUnoccludedRect(WindowInsetsCompat windowInsetsCompat) {
        // Do nothing if the window frame is empty, or there's no update from the cached insets, or
        // the root view size remains unchanged.
        WindowInsets windowInsets = windowInsetsCompat.toWindowInsets();
        Size windowSize = WindowInsetsUtils.getFrameFromInsets(windowInsets);
        if (windowSize.getWidth() == 0 && windowSize.getHeight() == 0) return false;

        Rect windowRect = new Rect(0, 0, windowSize.getWidth(), windowSize.getHeight());
        if (windowInsetsCompat.equals(mCachedInsets) && windowRect.equals(mWindowRect)) {
            return !mWidestUnoccludedRect.isEmpty();
        }

        mCachedInsets = windowInsetsCompat;
        mWindowRect.set(windowRect);

        updateWidestUnoccludedRect(windowInsetsCompat);

        // Notify observers about the update.
        for (Observer observer : mObservers) {
            observer.onBoundingRectsUpdated(mWidestUnoccludedRect);
        }
        return !mWidestUnoccludedRect.isEmpty();
    }

    /**
     * Determines the widest rect within the system inset region of the current inset type that is
     * not occluded by system controls.
     *
     * @param windowInsetsCompat The current system {@link WindowInsetsCompat}.
     */
    private void updateWidestUnoccludedRect(WindowInsetsCompat windowInsetsCompat) {
        Insets insets = windowInsetsCompat.getInsets(mInsetType);
        Rect insetRectInWindow = WindowInsetsUtils.toRectInWindow(mWindowRect, insets);
        if (!insetRectInWindow.isEmpty()) {
            mBoundingRects = getBoundingRectsFromInsets(windowInsetsCompat);
            mWidestUnoccludedRect =
                    WindowInsetsUtils.getWidestUnoccludedRect(insetRectInWindow, mBoundingRects);
        } else {
            mBoundingRects = List.of();
            mWidestUnoccludedRect = new Rect();
        }
    }
}
