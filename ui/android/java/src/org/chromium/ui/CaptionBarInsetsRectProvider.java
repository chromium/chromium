// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.view.View;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

// Note - this class does not currently support the case in which the status bar covers the caption
// bar, and extends past it (i.e. there is a larger status bar than caption bar).
// TODO(crbug.com/403349146): Add support for the edge case where the status bar extends below the
// caption bar.
@NullMarked
public class CaptionBarInsetsRectProvider extends InsetsRectProvider {
    int mOverlappingStatusBarHeight;

    /** This observer gets notified when the status bar-caption bar overlap size changes. */
    public interface SystemBarOverlapObserver {
        /**
         * Called when the status bar-caption bar overlap size changes.
         *
         * @param overlappingStatusBarHeight Height of the status bar. {@code 0} if the status bar
         *     does not overlap with the caption bar.
         */
        void onStatusBarOverlapChanged(int overlappingStatusBarHeight);
    }

    private final ObserverList<SystemBarOverlapObserver> mSystemBarOverlapObservers =
            new ObserverList<>();

    /**
     * Create a rect provider for caption bars, which understands the connection between caption bar
     * insets and the other system insets. This class should only be used for Android R+.
     *
     * @param insetObserver {@link InsetObserver} that's attached to the root view.
     * @param initialInsets The initial window insets that will be used to read the bounding rects.
     * @param insetConsumerSource The {@link InsetConsumerSource} of inset observation and
     */
    public CaptionBarInsetsRectProvider(
            InsetObserver insetObserver,
            @Nullable WindowInsetsCompat initialInsets,
            int insetConsumerSource) {
        super(
                insetObserver,
                WindowInsetsCompat.Type.captionBar(),
                initialInsets,
                insetConsumerSource);
    }

    @Override
    public WindowInsetsCompat onApplyWindowInsets(
            View view, WindowInsetsCompat windowInsetsCompat) {
        Insets rawCaptionBarInsets =
                windowInsetsCompat.getInsets(WindowInsetsCompat.Type.captionBar());

        WindowInsetsCompat processedCaptionBarInsets =
                super.onApplyWindowInsets(view, windowInsetsCompat);
        if (processedCaptionBarInsets
                .getInsets(WindowInsetsCompat.Type.captionBar())
                .equals(rawCaptionBarInsets)) {
            return processedCaptionBarInsets;
        }

        // Account for any consumed caption bar insets in the rest of the system bars insets. If and
        // when the caption bar overlaps with the status bar / navigation bar, we should also
        // consume these insets if caption bar insets are consumed to prevent other views from
        // incorrectly using insets from a consumed and overlapping system insets region.
        Insets consumedCaptionBarInsets =
                Insets.subtract(
                        rawCaptionBarInsets,
                        processedCaptionBarInsets.getInsets(WindowInsetsCompat.Type.captionBar()));

        Insets statusBarInsetsWithoutCaptionBar =
                Insets.max(
                        Insets.subtract(
                                processedCaptionBarInsets.getInsets(
                                        WindowInsetsCompat.Type.statusBars()),
                                consumedCaptionBarInsets),
                        Insets.NONE);

        int newOverlappingStatusBarHeight =
                statusBarInsetsWithoutCaptionBar.equals(Insets.NONE)
                        ? processedCaptionBarInsets.getInsets(WindowInsetsCompat.Type.statusBars())
                                .top
                        : 0;
        if (newOverlappingStatusBarHeight != mOverlappingStatusBarHeight) {
            mOverlappingStatusBarHeight = newOverlappingStatusBarHeight;
            for (SystemBarOverlapObserver observer : mSystemBarOverlapObservers) {
                observer.onStatusBarOverlapChanged(mOverlappingStatusBarHeight);
            }
        }

        Insets navigationBarInsetsWithoutCaptionBar =
                Insets.max(
                        Insets.subtract(
                                processedCaptionBarInsets.getInsets(
                                        WindowInsetsCompat.Type.navigationBars()),
                                consumedCaptionBarInsets),
                        Insets.NONE);
        return new WindowInsetsCompat.Builder(processedCaptionBarInsets)
                .setInsets(WindowInsetsCompat.Type.statusBars(), statusBarInsetsWithoutCaptionBar)
                .setInsets(
                        WindowInsetsCompat.Type.navigationBars(),
                        navigationBarInsetsWithoutCaptionBar)
                .build();
    }

    /**
     * Add an observer that gets notified when the status bar-caption bar overlap size changes.
     *
     * @param obs The observer to add.
     */
    public void addSystemBarOverlapObserver(SystemBarOverlapObserver obs) {
        mSystemBarOverlapObservers.addObserver(obs);
    }

    int getOverlappingStatusBarHeightForTesting() {
        return mOverlappingStatusBarHeight;
    }
}
