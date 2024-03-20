// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.graphics.Rect;
import android.graphics.Region;
import android.graphics.RegionIterator;

import androidx.annotation.NonNull;
import androidx.core.graphics.Insets;

import org.chromium.base.Callback;

import java.util.List;

/** Helper functions for working with WindowInsets and Rects. */
public final class WindowInsetsUtils {

    /** Private constructor to stop instantiation. */
    private WindowInsetsUtils() {}

    /**
     * Return the rect represented by the input {@link Insets}. Return an empty Rect if the input
     * |insets| inset more than one edge from the |windowRect| (i.e. has more than one non-zero
     * value over the four sides: left / top / right / bottom).
     *
     * <p><b>Example 1: </b><br>
     * windowRect = Rect(0, 0, 100, 50) // left: 0, top: 0, right: 100, bottom: 50 <br>
     * insets = (0, 20, 0, 0) // Inset 20 from the top. <br>
     * Output: Rect(0, 0, 100, 20) // Insets represent 20 from the top of the windowRect.
     *
     * <p><b>Example 2:</b> <br>
     * windowRect = Rect(0, 0, 100, 50) // left: 0, top: 0, right: 100, bottom: 50 <br>
     * insets = (0, 0, 30, 0) // Inset 30 from the right. <br>
     * Output: Rect(30, 0, 100, 50) // Insets represent 30 from the right of the windowRect,
     * starting from left=70
     *
     * <p><b>Example 3:</b> <br>
     * windowRect = Rect(0, 0, 100, 50) // left: 0, top: 0, right: 100, bottom: 50 <br>
     * insets = (0, 0, 10, 10) // Inset 10 from the right and 10 from bottom <br>
     * Output: Rect(0, 0, 0, 0) // Insets represent more than one edge and it's not an Rect.
     *
     * <p><b>Example 4:</b> <br>
     * windowRect = Rect(0, 0, 100, 50) // left: 0, top: 0, right: 100, bottom: 50 <br>
     * insets = (0, 0, 0, 0) // No insets from thw windowRect <br>
     * Output: Rect(0, 0, 0, 0) // Insets does not represent any rect.
     *
     * @param windowRect The rect representing the root view of the window.
     * @param insets Insets describing a certain type of a WindowInsts.
     * @return Rect that the insets represent in the windowRect. Empty rect if insets represent more
     *     than one edge.
     */
    public static @NonNull Rect toRectInWindow(@NonNull Rect windowRect, @NonNull Insets insets) {
        int sides = 0;
        Rect res = new Rect(windowRect);

        if (insets.left != 0) {
            res.right = windowRect.left + insets.left;
            sides++;
        }

        if (insets.top != 0) {
            if (sides > 0) return new Rect();
            res.bottom = windowRect.top + insets.top;
            sides++;
        }

        if (insets.right != 0) {
            if (sides > 0) return new Rect();
            res.left = windowRect.right - insets.right;
            sides++;
        }

        if (insets.bottom != 0) {
            if (sides > 0) return new Rect();
            res.top = windowRect.bottom - insets.bottom;
            sides++;
        }

        return sides == 1 ? res : new Rect();
    }

    /**
     * Get the Rect with the maximum width within the |regionRect| that is not blocked by any rects
     * within the |blockedRects|. This algorithm only prioritizes the width of the returned Rects,
     * so the returned area does not necessarily have the maximum area. If there are multiple rects
     * with the same width, this method will bias the first Rect found in the region.
     *
     * @see Region
     * @see RegionIterator
     * @param regionRect The un-blocked rect area.
     * @param blockedRects Areas within the regionRect that are blocked.
     * @return The widest Rect seen in the regionRect that's not blocked by any blockedRects.
     */
    public static @NonNull Rect getWidestUnoccludedRect(
            @NonNull Rect regionRect, List<Rect> blockedRects) {
        if (regionRect.isEmpty()) return regionRect;

        Region region = new Region(regionRect);
        for (Rect rect : blockedRects) {
            region.op(rect, Region.Op.DIFFERENCE);
        }
        Rect widestUnoccludedRect = new Rect();
        forEachRect(
                region,
                (rect) -> {
                    if (widestUnoccludedRect.width() < rect.width()) {
                        widestUnoccludedRect.set(rect);
                    }
                });
        return widestUnoccludedRect;
    }

    private static void forEachRect(Region region, Callback<Rect> rectConsumer) {
        final RegionIterator it = new RegionIterator(region);
        final Rect rect = new Rect();
        while (it.next(rect)) {
            rectConsumer.onResult(rect);
        }
    }
}
