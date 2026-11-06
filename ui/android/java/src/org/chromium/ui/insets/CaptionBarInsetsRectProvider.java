// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.insets;

import android.graphics.Rect;
import android.view.View;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

// Note - this class does not currently support the case in which the status bar covers the caption
// bar, and extends past it (i.e. there is a larger status bar than caption bar).
// TODO(crbug.com/403349146): Add support for the edge case where the status bar extends below the
// caption bar.
@NullMarked
public class CaptionBarInsetsRectProvider extends InsetsRectProvider {
    /**
     * Create a rect provider for caption bars, which understands the connection between caption bar
     * insets and the other system insets. This class should only be used for Android R+.
     *
     * @param insetObserver {@link InsetObserver} that's attached to the root view.
     * @param initialInsets The initial window insets that will be used to read the bounding rects.
     * @param insetConsumerSource The {@link InsetConsumerSource} of inset observation and
     *     consumption.
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
        Insets rawCaptionBarInsets = getInsets(windowInsetsCompat);

        WindowInsetsCompat processedCaptionBarInsets =
                super.onApplyWindowInsets(view, windowInsetsCompat);
        if (getInsets(processedCaptionBarInsets).equals(rawCaptionBarInsets)) {
            return processedCaptionBarInsets;
        }

        // Account for any consumed caption bar insets in the rest of the system bars insets. If and
        // when the caption bar overlaps with the status bar / navigation bar, we should also
        // consume these insets if caption bar insets are consumed to prevent other views from
        // incorrectly using insets from a consumed and overlapping system insets region.
        Insets consumedCaptionBarInsets =
                Insets.subtract(rawCaptionBarInsets, getInsets(processedCaptionBarInsets));

        Insets statusBarInsetsWithoutCaptionBar =
                Insets.max(
                        Insets.subtract(
                                processedCaptionBarInsets.getInsets(
                                        WindowInsetsCompat.Type.statusBars()),
                                consumedCaptionBarInsets),
                        Insets.NONE);

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

    @Override
    protected Insets getInsets(WindowInsetsCompat windowInsetsCompat) {
        // A fullscreen desktop window reports an invisible caption bar until the app opts into
        // header customization, so read the insets ignoring visibility while the status bar
        // (or the caption bar itself) is visible. When both are hidden, the window is in
        // immersive fullscreen and the visible (empty) insets must be used so that desktop
        // windowing mode is exited. See crbug.com/555401309.
        if (windowInsetsCompat.isVisible(WindowInsetsCompat.Type.statusBars())
                || windowInsetsCompat.isVisible(WindowInsetsCompat.Type.captionBar())) {
            return windowInsetsCompat.getInsetsIgnoringVisibility(
                    WindowInsetsCompat.Type.captionBar());
        }
        return windowInsetsCompat.getInsets(WindowInsetsCompat.Type.captionBar());
    }

    /**
     * Gets the system bounding rects for the caption bar ignoring visibility so an invisible
     * fullscreen header is still measured. When status bar insets are present, it means that there
     * is a potential caption-status overlap. In this case, the caption bar should include the
     * status bar region as a bounding rect so that it is considered as occluded space within the
     * caption bar, in addition to the bounding rects that define the caption bar system UI.
     *
     * @param windowInsetsCompat The current system {@link WindowInsetsCompat}.
     * @return The list of system bounding rects within the caption bar.
     */
    @Override
    protected List<Rect> getBoundingRectsFromInsets(WindowInsetsCompat windowInsetsCompat) {
        var systemCaptionBoundingRects =
                WindowInsetsUtils.getBoundingRectsIgnoringVisibilityFromInsets(
                        windowInsetsCompat.toWindowInsets(), WindowInsetsCompat.Type.captionBar());
        Rect statusBarRect =
                WindowInsetsUtils.toRectInWindow(
                        getWindowRect(),
                        windowInsetsCompat.getInsets(WindowInsetsCompat.Type.statusBars()));
        if (statusBarRect.isEmpty()) return systemCaptionBoundingRects;

        // Non-zero status bar insets indicates that there is a potential status-caption overlap.
        // Include the status bar as a bounding rect for the caption bar.
        List<Rect> captionBoundingRects = new ArrayList<>(systemCaptionBoundingRects);
        captionBoundingRects.add(statusBarRect);
        return captionBoundingRects;
    }
}
