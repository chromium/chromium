// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Region;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;

import androidx.core.graphics.drawable.RoundedBitmapDrawable;
import androidx.core.graphics.drawable.RoundedBitmapDrawableFactory;

import org.chromium.base.TraceEvent;

/** A utility class that has helper methods for Android view. */
public final class ViewUtils {
    private static final int[] sLocationTmp = new int[2];

    // Prevent instantiation.
    private ViewUtils() {}

    /**
     * @return {@code true} if the given view has a focus.
     */
    public static boolean hasFocus(View view) {
        // If the container view is not focusable, we consider it always focused from
        // Chromium's point of view.
        return !isFocusable(view) ? true : view.hasFocus();
    }

    /**
     * Requests focus on the given view.
     *
     * @param view A {@link View} to request focus on.
     */
    public static void requestFocus(View view) {
        if (isFocusable(view) && !view.isFocused()) view.requestFocus();
    }

    private static boolean isFocusable(View view) {
        return view.isInTouchMode() ? view.isFocusableInTouchMode() : view.isFocusable();
    }

    /** Invalidates a view and all of its descendants. */
    private static void recursiveInvalidate(View view) {
        view.invalidate();
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            int childCount = group.getChildCount();
            for (int i = 0; i < childCount; i++) {
                View child = group.getChildAt(i);
                if (child.getVisibility() == View.VISIBLE) {
                    recursiveInvalidate(child);
                }
            }
        }
    }

    /** Sets the enabled property of a View and all of its descendants. */
    public static void setEnabledRecursive(View view, boolean enabled) {
        view.setEnabled(enabled);
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) {
                setEnabledRecursive(group.getChildAt(i), enabled);
            }
        }
    }

    /** Captures a bitmap of a View and draws it to a Canvas. */
    public static void captureBitmap(View view, Canvas canvas) {
        // Invalidate all the descendants of view, before calling view.draw(). Otherwise, some of
        // the descendant views may optimize away their drawing. http://crbug.com/415251
        recursiveInvalidate(view);
        view.draw(canvas);
    }

    /**
     * Return the position of {@code childView} relative to {@code rootView}.  {@code childView}
     * must be a child of {@code rootView}.  This returns the relative layout position, which does
     * not include translations.
     * @param rootView    The parent of {@code childView} to calculate the position relative to.
     * @param childView   The {@link View} to calculate the position of.
     * @param outPosition The resulting position with the format [x, y].
     */
    public static void getRelativeLayoutPosition(View rootView, View childView, int[] outPosition) {
        assert outPosition.length == 2;
        outPosition[0] = 0;
        outPosition[1] = 0;
        if (rootView == null || childView == rootView) return;
        while (childView != null) {
            outPosition[0] += childView.getLeft();
            outPosition[1] += childView.getTop();
            if (childView.getParent() == rootView) break;
            childView = (View) childView.getParent();
        }
    }

    /**
     * Return the position of {@code childView} relative to {@code rootView}.  {@code childView}
     * must be a child of {@code rootView}.  This returns the relative draw position, which includes
     * translations.
     * @param rootView    The parent of {@code childView} to calculate the position relative to.
     * @param childView   The {@link View} to calculate the position of.
     * @param outPosition The resulting position with the format [x, y].
     */
    public static void getRelativeDrawPosition(View rootView, View childView, int[] outPosition) {
        assert outPosition.length == 2;
        outPosition[0] = 0;
        outPosition[1] = 0;
        if (rootView == null || childView == rootView) return;
        while (childView != null) {
            outPosition[0] = (int) (outPosition[0] + childView.getX());
            outPosition[1] = (int) (outPosition[1] + childView.getY());
            if (childView.getParent() == rootView) break;
            childView = (View) childView.getParent();
        }
    }

    /**
     * Helper for overriding {@link ViewGroup#gatherTransparentRegion} for views that are fully
     * opaque and have children extending beyond their bounds. If the transparent region
     * optimization is turned on (which is the case whenever the view hierarchy contains a
     * SurfaceView somewhere), the children might otherwise confuse the SurfaceFlinger.
     */
    public static void gatherTransparentRegionsForOpaqueView(View view, Region region) {
        view.getLocationInWindow(sLocationTmp);
        region.op(
                sLocationTmp[0],
                sLocationTmp[1],
                sLocationTmp[0] + view.getRight() - view.getLeft(),
                sLocationTmp[1] + view.getBottom() - view.getTop(),
                Region.Op.DIFFERENCE);
    }

    /**
     *  Converts density-independent pixels (dp) to pixels on the screen (px).
     *
     *  @param dp Density-independent pixels are based on the physical density of the screen.
     *  @return   The physical pixels on the screen which correspond to this many
     *            density-independent pixels for this screen.
     */
    public static int dpToPx(Context context, float dp) {
        return dpToPx(context.getResources().getDisplayMetrics(), dp);
    }

    /**
     *  Converts density-independent pixels (dp) to pixels on the screen (px).
     *
     *  @param dp Density-independent pixels are based on the physical density of the screen.
     *  @return   The physical pixels on the screen which correspond to this many
     *            density-independent pixels for this screen.
     */
    public static int dpToPx(DisplayMetrics metrics, float dp) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, dp, metrics));
    }

    /**
     * Sets clip children for the provided ViewGroup and all of its ancestors.
     * @param view The ViewGroup whose children should (not) be clipped.
     * @param clip Whether to clip children to the parent bounds.
     */
    public static void setAncestorsShouldClipChildren(ViewGroup view, boolean clip) {
        ViewGroup parent = view;
        while (parent != null) {
            parent.setClipChildren(clip);
            if (!(parent.getParent() instanceof ViewGroup)) break;
            if (parent.getId() == android.R.id.content) break;
            parent = (ViewGroup) parent.getParent();
        }
    }

    /**
     * Creates a {@link RoundedBitmapDrawable} using the provided {@link Bitmap} and cornerRadius.
     * @param resources The {@link Resources}.
     * @param icon The {@link Bitmap} to round.
     * @param cornerRadius The corner radius.
     * @return A {@link RoundedBitmapDrawable} for the provided {@link Bitmap}.
     */
    public static RoundedBitmapDrawable createRoundedBitmapDrawable(
            Resources resources, Bitmap icon, int cornerRadius) {
        RoundedBitmapDrawable roundedIcon = RoundedBitmapDrawableFactory.create(resources, icon);
        roundedIcon.setCornerRadius(cornerRadius);
        return roundedIcon;
    }

    /**
     * Translates the canvas to ensure the specified view's coordinates are at 0, 0.
     *
     * @param from The view the canvas is currently translated to.
     * @param to The view to translate to.
     * @param canvas The canvas to be translated.
     *
     * @throws IllegalArgumentException if {@code from} is not an ancestor of {@code to}.
     */
    public static void translateCanvasToView(View from, View to, Canvas canvas)
            throws IllegalArgumentException {
        assert from != null;
        assert to != null;
        while (to != from) {
            canvas.translate(to.getLeft(), to.getTop());
            if (!(to.getParent() instanceof View)) {
                throw new IllegalArgumentException("View 'to' was not a desendent of 'from'.");
            }
            to = (View) to.getParent();
        }
    }

    /**
     * A wrapper that calls android.view.requestLayout() immediately after emitting an instant trace
     * event with information about the caller. This helps jank investigations, as there are traces
     * with lots of re-layouts, but we don't know where these layouts are coming from.
     *
     * Do not include any identifying information in {@code caller}! Only constant string literals
     * about code execution should be included.
     *
     * Examples of useful {@code caller} values:
     * "MyClass.myMethod" when requestLayout is called in myMethod in MyClass.
     * "MyClass.MyInnerClass.myMethod" when requestLayout is called in an inner class.
     * "MyClass.myMethod.MyInnerClass.innerMethod" when requestLayout is called in an inner class
     * defined in a method.
     * "MyClass.myMethod other_helpful_information" when requestLayout is called more than once in
     * the same method.
     * "MyClass.myMethod Runnable" when requestLayout is posted somewhere to be executed later.
     *
     * @param view The view to call requestLayout on.
     * @param caller Some queryable information about the caller.
     */
    // The trace event name is always a method/class name literal.
    @SuppressWarnings("NoDynamicStringsInTraceEventCheck")
    public static void requestLayout(View view, String caller) {
        assert view != null;
        TraceEvent.instant("requestLayout caller: " + caller);
        view.requestLayout();
    }
}
