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

import androidx.annotation.NonNull;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsCompat.Type.InsetsType;

import org.chromium.base.ObserverList;
import org.chromium.build.BuildConfig;
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
 * <li>1. Android version is atLeastV.
 * <li>2. WindowInsets of given type has insets from one side exactly.
 */
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

    private WindowInsetsCompat mCachedInsets;
    private List<Rect> mBoundingRects;
    private Rect mWidestUnoccludedRect = new Rect();

    /**
     * Create a rect provider for a specific inset type. This class should only be used for Android
     * V+.
     *
     * @param insetObserver {@link InsetObserver} that's attached to the root view.
     * @param insetType {@link InsetsType} this provider is observing.
     * @param initialInsets The initial window insets that will be used to read the bounding rects.
     */
    public InsetsRectProvider(
            @NonNull InsetObserver insetObserver,
            @InsetsType int insetType,
            WindowInsetsCompat initialInsets) {
        mInsetType = insetType;
        mBoundingRects = List.of();
        mInsetObserver = insetObserver;

        // TODO (crbug/325351108): Remove the test check once we support Android V testing.
        assert BuildConfig.IS_FOR_TEST || VERSION.SDK_INT >= VERSION_CODES.VANILLA_ICE_CREAM;
        mInsetObserver.addInsetsConsumer(this);
        if (initialInsets != null) {
            updateWidestUnoccludedRect(initialInsets);
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
    @NonNull
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
    @NonNull
    @Override
    public WindowInsetsCompat onApplyWindowInsets(
            @NonNull View view, @NonNull WindowInsetsCompat windowInsetsCompat) {
        // Ignore the input by version check.
        // TODO (crbug/351389242): Remove the test check once we support Android V testing.
        if (!BuildConfig.IS_FOR_TEST && VERSION.SDK_INT < VERSION_CODES.VANILLA_ICE_CREAM) {
            return windowInsetsCompat;
        }

        updateWidestUnoccludedRect(windowInsetsCompat);
        return new WindowInsetsCompat.Builder(windowInsetsCompat)
                .setInsets(mInsetType, Insets.NONE)
                .build();
    }

    private void updateWidestUnoccludedRect(WindowInsetsCompat windowInsetsCompat) {
        // Do nothing if there's no update from the cached insets, or the root view size remains
        // unchanged.
        WindowInsets windowInsets = windowInsetsCompat.toWindowInsets();
        Size windowSize = WindowInsetsUtils.getFrameFromInsets(windowInsets);
        Rect windowRect = new Rect(0, 0, windowSize.getWidth(), windowSize.getHeight());
        if (windowInsetsCompat.equals(mCachedInsets) && windowRect.equals(mWindowRect)) return;

        mCachedInsets = windowInsetsCompat;
        mWindowRect.set(windowRect);

        Insets insets = windowInsetsCompat.getInsets(mInsetType);
        Rect insetRectInWindow = WindowInsetsUtils.toRectInWindow(mWindowRect, insets);
        if (!insetRectInWindow.isEmpty()) {
            mBoundingRects = WindowInsetsUtils.getBoundingRectsFromInsets(windowInsets, mInsetType);
            mWidestUnoccludedRect =
                    WindowInsetsUtils.getWidestUnoccludedRect(insetRectInWindow, mBoundingRects);
        } else {
            mBoundingRects = List.of();
            mWidestUnoccludedRect = new Rect();
        }

        // Notify observers about the update.
        for (Observer observer : mObservers) {
            observer.onBoundingRectsUpdated(mWidestUnoccludedRect);
        }
    }
}
