// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.insets;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Region;
import android.graphics.RegionIterator;
import android.util.DisplayMetrics;
import android.util.Size;
import android.view.WindowInsets;
import android.view.WindowManager;

import androidx.core.graphics.Insets;
import androidx.core.view.DisplayCutoutCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsCompat.Type;
import androidx.core.view.WindowInsetsCompat.Type.InsetsType;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Comparator;
import java.util.List;

/** Helper functions for working with WindowInsets and Rects. */
@NullMarked
public final class WindowInsetsUtils {
    private static final String TAG = "WindowInsetsUtils";

    private static final Size DEFAULT_INSETS_FRAME = new Size(0, 0);
    private static final List<Rect> DEFAULT_INSETS_BOUNDING_RECTS = List.of();

    // When a window is expanded, updates to the frame and the bounding rects for the window insets
    // are not synchronized. This can cause issues, such as confusing the desktop windowing
    // heuristics that impact caption bar customization. To account for this, bounding rects will be
    // corrected to match the window frame width, as long as the difference between the bounding
    // rects and frame width are within this threshold.
    // TODO(crbug.com/443865885): Remove this once bounding rects are synchronized with window
    //  configuration changes.
    private static final float EXPANDING_WINDOW_GUTTER_BOUNDING_RECT_THRESHOLD = 0.1f;

    private static boolean sGetFrameMethodNotFound;
    private static boolean sGetBoundingRectsMethodNotFound;

    private static @Nullable Size sFrameForTesting;
    private static @Nullable Rect sWidestUnoccludedRectForTesting;
    private static @Nullable List<Rect> sBoundingRectsForTesting;
    private static boolean sUnoccludedRegionComplexForTesting;

    /**
     * Class to encapsulate information about the uncoccluded region determined by {@link
     * #getUnoccludedRegion(Rect, List)}.
     */
    public static class UnoccludedRegion {
        private final Rect mWidestUnoccludedRect;
        private final boolean mIsRegionComplex;

        /* Constructor to get the unoccluded region info. */
        public UnoccludedRegion(Rect widestUnoccludedRect, boolean isRegionComplex) {
            mWidestUnoccludedRect = widestUnoccludedRect;
            mIsRegionComplex = isRegionComplex;
        }

        /**
         * @return The widest rect in the unoccluded region. See docs for {@link
         *     #getUnoccludedRegion(Rect, List)} for more details.
         */
        public Rect getWidestUnoccludedRect() {
            return mWidestUnoccludedRect;
        }

        /**
         * @return {@code true} if the unoccluded region is complex and contains multiple unoccluded
         *     rects, {@code false} if it is empty or contains a single unoccluded rect.
         */
        public boolean isRegionComplex() {
            return mIsRegionComplex;
        }
    }

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
    public static Rect toRectInWindow(Rect windowRect, Insets insets) {
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
     * Get the {@link UnoccludedRegion} within the |regionRect|. This includes the rect with the
     * maximum width that is not blocked by any rects within the |blockedRects|. This algorithm only
     * prioritizes the width of the returned rects, so the returned area does not necessarily have
     * the maximum area. If there are multiple rects with the same width, this method will bias the
     * first rect found in the region. If |blockedRects| is empty, this method will return an empty
     * rect.
     *
     * @see Region
     * @see RegionIterator
     * @param regionRect The un-blocked rect area.
     * @param blockedRects Areas within the regionRect that are blocked.
     * @return The {@link UnoccludedRegion} in |regionRect| that is not blocked by any
     *     |blockedRects|.
     */
    public static UnoccludedRegion getUnoccludedRegion(Rect regionRect, List<Rect> blockedRects) {
        if (sWidestUnoccludedRectForTesting != null) {
            return new UnoccludedRegion(
                    sWidestUnoccludedRectForTesting, sUnoccludedRegionComplexForTesting);
        }
        if (blockedRects.isEmpty()) {
            return new UnoccludedRegion(new Rect(), /* isRegionComplex= */ false);
        }

        Region region = new Region(regionRect);
        for (Rect rect : blockedRects) {
            region.op(rect, Region.Op.DIFFERENCE);
        }

        if (region.isEmpty()) {
            return new UnoccludedRegion(new Rect(), /* isRegionComplex= */ false);
        }

        Rect widestUnoccludedRect = new Rect();
        forEachRect(
                region,
                (rect) -> {
                    if (widestUnoccludedRect.width() < rect.width()) {
                        widestUnoccludedRect.set(rect);
                    }
                });
        return new UnoccludedRegion(widestUnoccludedRect, region.isComplex());
    }

    /** See {@link WindowInsets#getFrame()} for details. */
    @SuppressWarnings("NewApi")
    public static Size getFrameFromInsets(@Nullable WindowInsets windowInsets) {
        if (sFrameForTesting != null) return sFrameForTesting;

        // This invocation is wrapped in a try-catch block to allow backporting of the #getFrame()
        // API on pre-V devices. On pre-V devices not supporting this API, a default value will be
        // cached on the first failure and returned subsequently.
        if (sGetFrameMethodNotFound) return DEFAULT_INSETS_FRAME;
        try {
            return windowInsets == null ? DEFAULT_INSETS_FRAME : windowInsets.getFrame();
        } catch (NoSuchMethodError e) {
            Log.w(TAG, e.toString());
            sGetFrameMethodNotFound = true;
            return DEFAULT_INSETS_FRAME;
        }
    }

    /**
     * See {@link WindowInsets#getBoundingRects(int)} for details. Note that this specific method
     * may make modifications to the bounding rects to correct for window inset updates where the
     * bounding rects and window frame are not synchronized.
     */
    @SuppressWarnings("NewApi")
    public static List<Rect> getBoundingRectsFromInsets(
            @Nullable WindowInsets windowInsets, @InsetsType int insetType) {
        if (sBoundingRectsForTesting != null) return sBoundingRectsForTesting;

        // This invocation is wrapped in a try-catch block to allow backporting of the
        // #getBoundingRects() API on pre-V devices. On pre-V devices not supporting this API, a
        // default value will be cached on the first failure and returned subsequently.
        if (sGetBoundingRectsMethodNotFound) return DEFAULT_INSETS_BOUNDING_RECTS;

        try {
            if (windowInsets == null) return DEFAULT_INSETS_BOUNDING_RECTS;
            return maybeCorrectStartAndEndRects(
                    windowInsets.getBoundingRects(insetType), getFrameFromInsets(windowInsets));
        } catch (NoSuchMethodError e) {
            Log.w(TAG, e.toString());
            sGetBoundingRectsMethodNotFound = true;
            return DEFAULT_INSETS_BOUNDING_RECTS;
        }
    }

    /**
     * When a window is being resized, updates in the {@link WindowInsets} values for the window
     * frame (height, width) are not synchronized to changes in the bounding rects. This causes
     * issues, as the laggy update to the bounding rects gives the impression of unoccluded space on
     * the right side. This corrects the leftmost and rightmost bounding rects to align with the
     * edges of the window frame, as long as they are within a certain threshold.
     */
    private static List<Rect> maybeCorrectStartAndEndRects(
            List<Rect> boundingRects, Size windowFrame) {
        if (boundingRects.size() < 1) return boundingRects;

        float differenceThreshold =
                windowFrame.getWidth() * EXPANDING_WINDOW_GUTTER_BOUNDING_RECT_THRESHOLD;

        boundingRects.sort(Comparator.comparingInt(rect -> rect.left));
        Rect startRect = boundingRects.get(0);
        startRect.left = startRect.left <= differenceThreshold ? 0 : startRect.left;

        boundingRects.sort(Comparator.comparingInt(rect -> rect.right));
        Rect endRect = boundingRects.get(boundingRects.size() - 1);
        if (endRect.right != windowFrame.getWidth()) {
            int widthDiff = Math.abs(endRect.right - windowFrame.getWidth());
            if (widthDiff <= differenceThreshold) {
                endRect.right = windowFrame.getWidth();
            }
        }
        return boundingRects;
    }

    /** Sets the window frame size for testing purposes. */
    public static void setFrameForTesting(Size frame) {
        sFrameForTesting = frame;
        ResettersForTesting.register(() -> sFrameForTesting = null);
    }

    /** Sets a rect to be returned by {@code #getWidestUnoccludedRect()} for testing purposes. */
    public static void setWidestUnoccludedRectForTesting(Rect widestUnoccludedRect) {
        sWidestUnoccludedRectForTesting = widestUnoccludedRect;
        ResettersForTesting.register(() -> sWidestUnoccludedRectForTesting = null);
    }

    /** Sets the bounding rects for testing purposes. */
    public static void setBoundingRectsForTesting(List<Rect> boundingRects) {
        sBoundingRectsForTesting = boundingRects;
        ResettersForTesting.register(() -> sBoundingRectsForTesting = null);
    }

    /** Sets whether the unoccluded region is complex for testing purposes. */
    public static void setUnoccludedRegionComplexForTesting(boolean isComplex) {
        sUnoccludedRegionComplexForTesting = isComplex;
        ResettersForTesting.register(() -> sUnoccludedRegionComplexForTesting = false);
    }

    /** Returns whether the insets has a non-zero left, right, or bottom inset. */
    public static boolean hasOneNonZeroInsetExcludingTop(Insets insets) {
        return (insets.bottom > 0 && insets.left == 0 && insets.right == 0)
                || (insets.bottom == 0 && insets.left > 0 && insets.right == 0)
                || (insets.bottom == 0 && insets.left == 0 && insets.right > 0);
    }

    /** Returns a copy of given rectangle that has been inset with given insets. */
    public static Rect insetRectangle(Rect rectangle, Insets insets) {
        final Rect output = new Rect(rectangle);
        output.left += insets.left;
        output.top += insets.top;
        output.right -= insets.right;
        output.bottom -= insets.bottom;
        return output;
    }

    private static void forEachRect(Region region, Callback<Rect> rectConsumer) {
        final RegionIterator it = new RegionIterator(region);
        final Rect rect = new Rect();
        while (it.next(rect)) {
            rectConsumer.onResult(rect);
        }
    }

    // Determine if padding is necessary according to WindowManager.LayoutParams. This is intended
    // to keep the behavior for Android 15-.
    // Ref: https://developer.android.com/develop/ui/views/layout/display-cutout
    public static boolean shouldPadDisplayCutout(
            @Nullable WindowInsetsCompat insets, Context context) {
        if (insets == null) return true;

        Activity activity = ContextUtils.activityFromContext(context);
        if (activity == null) {
            Log.w(TAG, "should not receive window insets in non-activity context.");
            return false;
        }

        DisplayCutoutCompat cutout = insets.getDisplayCutout();
        if (cutout == null) return false;

        int cutoutMode = activity.getWindow().getAttributes().layoutInDisplayCutoutMode;
        switch (cutoutMode) {
            case WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS:
                return false;
            case WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER:
                return true;
            case WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES:
                // For web compatibility, we should add padding when insets does not overlap with
                // system bars.
                DisplayMetrics displayMetrics = activity.getResources().getDisplayMetrics();
                boolean isPortrait = displayMetrics.widthPixels < displayMetrics.heightPixels;

                if (isPortrait) {
                    // width < height, top / bottom are the short edge.
                    return cutout.getSafeInsetLeft() > 0 || cutout.getSafeInsetRight() > 0;
                }
                // else: height > width, left / right are the short edges
                return cutout.getSafeInsetTop() > 0 || cutout.getSafeInsetBottom() > 0;

            default: // LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT
                assert cutoutMode
                        == WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
                // From Android's doc: The window is allowed to extend into the DisplayCutout area,
                // only if the DisplayCutout is fully contained within a system bar or the
                // DisplayCutout is not deeper than 16 dp.

                Insets systemInsets = insets.getInsets(Type.systemBars());
                Insets systemAndCutoutInsets =
                        insets.getInsets(Type.systemBars() + Type.displayCutout());
                if (systemInsets.equals(systemAndCutoutInsets)) {
                    return false;
                }

                float density = activity.getResources().getDisplayMetrics().density;
                RectF rect =
                        new RectF(
                                cutout.getSafeInsetLeft() / density,
                                cutout.getSafeInsetTop() / density,
                                cutout.getSafeInsetRight() / density,
                                cutout.getSafeInsetBottom() / density);
                return (rect.left > 16 || rect.top > 16 || rect.right > 16 || rect.bottom > 16);
        }
    }
}
