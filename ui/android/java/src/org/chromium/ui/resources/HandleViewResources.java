// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.ui.R;

/** Helper class for retrieving resources related to selection handles. */
@JNINamespace("ui")
public class HandleViewResources {
    // Android handle drawables have a transparent horizontal padding,
    // which is one-fourth of the image. This variable is to take the
    // padding ratio into account while calculating the handle origin position.
    private static final float HANDLE_HORIZONTAL_PADDING_RATIO = 0.25f;

    private static final int[] LEFT_HANDLE_ATTRS = {
        android.R.attr.textSelectHandleLeft,
    };

    private static final int[] CENTER_HANDLE_ATTRS = {
        android.R.attr.textSelectHandle,
    };

    private static final int[] RIGHT_HANDLE_ATTRS = {
        android.R.attr.textSelectHandleRight,
    };

    public static Drawable getLeftHandleDrawable(Context context) {
        return getHandleDrawable(context, LEFT_HANDLE_ATTRS);
    }

    public static Drawable getCenterHandleDrawable(Context context) {
        return getHandleDrawable(context, CENTER_HANDLE_ATTRS);
    }

    public static Drawable getRightHandleDrawable(Context context) {
        return getHandleDrawable(context, RIGHT_HANDLE_ATTRS);
    }

    private static Drawable getHandleDrawable(Context context, final int[] attrs) {
        TypedArray a = context.getTheme().obtainStyledAttributes(attrs);
        Drawable drawable = a.getDrawable(0);
        if (drawable == null) {
            // If themed resource lookup fails, fall back to using the Context's
            // resources for attribute lookup.
            try {
                drawable =
                        ApiCompatibilityUtils.getDrawable(
                                context.getResources(), a.getResourceId(0, 0));
            } catch (Resources.NotFoundException e) {
                // The caller should handle the null return case appropriately.
            }
        }
        a.recycle();
        return drawable;
    }

    private static Bitmap getHandleBitmap(Context activityContext, final int[] attrs) {
        // TODO(jdduke): Properly derive and apply theme color.
        final ContextThemeWrapper context =
                new ContextThemeWrapper(
                        activityContext == null
                                ? ContextUtils.getApplicationContext()
                                : activityContext,
                        R.style.ThemeOverlay_UI_SelectionHandle);
        TypedArray a = context.getTheme().obtainStyledAttributes(attrs);
        final int resId = a.getResourceId(a.getIndex(0), 0);
        final Resources res = a.getResources();
        a.recycle();

        final Bitmap.Config config = Bitmap.Config.ARGB_8888;
        final BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = false;
        options.inPreferredConfig = config;
        Bitmap bitmap = BitmapFactory.decodeResource(res, resId, options);
        if (bitmap != null) return bitmap;

        // If themed resource lookup fails, fall back to using the Context's
        // resources for attribute lookup.
        if (res != context.getResources()) {
            bitmap = BitmapFactory.decodeResource(context.getResources(), resId, options);
            if (bitmap != null) return bitmap;
        }

        Drawable drawable = getHandleDrawable(context, attrs);
        assert drawable != null;

        final int width = drawable.getIntrinsicWidth();
        final int height = drawable.getIntrinsicHeight();
        Bitmap canvasBitmap = Bitmap.createBitmap(width, height, config);
        Canvas canvas = new Canvas(canvasBitmap);
        drawable.setBounds(0, 0, width, height);
        drawable.draw(canvas);
        return canvasBitmap;
    }

    @CalledByNative
    public static float getHandleHorizontalPaddingRatio() {
        return HANDLE_HORIZONTAL_PADDING_RATIO;
    }

    @CalledByNative
    private static Bitmap getLeftHandleBitmap(Context context) {
        return getHandleBitmap(context, LEFT_HANDLE_ATTRS);
    }

    @CalledByNative
    private static Bitmap getCenterHandleBitmap(Context context) {
        return getHandleBitmap(context, CENTER_HANDLE_ATTRS);
    }

    @CalledByNative
    private static Bitmap getRightHandleBitmap(Context context) {
        return getHandleBitmap(context, RIGHT_HANDLE_ATTRS);
    }
}
