// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.ClipData;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build.VERSION_CODES;
import android.util.DisplayMetrics;
import android.util.Pair;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.compat.ApiHelperForN;
import org.chromium.ui.R;

/**
 * Drag and drop helper class in charge of building the clip data, wrapping calls to
 * {@link android.view.View#startDragAndDrop}. Also used for mocking out real function calls to
 * Android.
 */
class DragAndDropDelegateImpl implements ViewAndroidDelegate.DragAndDropDelegate, DragStateTracker {
    private int mShadowWidth;
    private int mShadowHeight;
    private boolean mIsDragStarted;

    // Implements ViewAndroidDelegate.DragAndDropDelegate
    /**
     * Wrapper to call {@link android.view.View#startDragAndDrop}.
     * @see ViewAndroidDelegate#startDragAndDrop(Bitmap, DropDataAndroid)
     * */
    @Override
    @RequiresApi(api = VERSION_CODES.N)
    public boolean startDragAndDrop(@NonNull View containerView, @NonNull Bitmap shadowImage,
            @NonNull DropDataAndroid dropData) {
        mIsDragStarted = true;

        ClipData clipdata = buildClipData(dropData);
        if (clipdata == null) {
            return false;
        }
        return ApiHelperForN.startDragAndDrop(containerView, clipdata,
                createDragShadowBuilder(
                        containerView.getContext(), shadowImage, dropData.hasImage()),
                null, buildFlags(dropData));
    }

    // Implements DragStateTracker
    @Override
    public boolean isDragStarted() {
        return mIsDragStarted;
    }

    @Override
    public int getDragShadowWidth() {
        return mShadowWidth;
    }

    @Override
    public int getDragShadowHeight() {
        return mShadowHeight;
    }

    @Override
    public void destroy() {
        reset();
    }

    // Implements View.OnDragListener
    @Override
    public boolean onDrag(View view, DragEvent dragEvent) {
        if (dragEvent.getAction() == DragEvent.ACTION_DRAG_ENDED) {
            reset();
            if (!dragEvent.getResult()) {
                // Clear the image data immediately when not used.
                DropDataContentProvider.clearCache();
                // TODO: add metric
            } else {
                // Otherwise, clear it with a delay to allow asynchronous data transfer.
                DropDataContentProvider.clearCacheWithDelay();
                // TODO: add metric
            }
        }
        // Return false so this listener does not consume the drag event of the view it listened to.
        return false;
    }

    protected ClipData buildClipData(DropDataAndroid dropData) {
        if (dropData.isPlainText()) {
            return ClipData.newPlainText(null, dropData.text);
        } else if (dropData.hasImage()) {
            Uri uri = DropDataContentProvider.cache(
                    dropData.imageContent, dropData.imageContentExtension);
            return ClipData.newUri(
                    ContextUtils.getApplicationContext().getContentResolver(), null, uri);
            // TODO: ensure MIME type of ClipData is correct
        } else {
            // TODO(crbug.com/1289393): handle link dragging
            return null;
        }
    }

    @RequiresApi(api = VERSION_CODES.N)
    protected int buildFlags(DropDataAndroid dropData) {
        if (dropData.isPlainText()) {
            return View.DRAG_FLAG_GLOBAL;
        } else if (dropData.hasImage()) {
            return View.DRAG_FLAG_GLOBAL | View.DRAG_FLAG_GLOBAL_URI_READ;
        } else {
            return 0;
        }
    }

    protected View.DragShadowBuilder createDragShadowBuilder(
            Context context, Bitmap shadowImage, boolean isImage) {
        ImageView imageView = new ImageView(context);
        imageView.setImageBitmap(shadowImage);

        if (isImage) {
            Pair<Integer, Integer> widthHeight =
                    getWidthHeightForScaleDragShadow(context, shadowImage);
            mShadowWidth = widthHeight.first;
            mShadowHeight = widthHeight.second;
        } else {
            mShadowWidth = shadowImage.getWidth();
            mShadowHeight = shadowImage.getHeight();
        }
        imageView.layout(0, 0, mShadowWidth, mShadowHeight);

        return new DragShadowBuilder(imageView);
    }

    // TODO(crbug.com/1295868): Scale image in C++ before passing into Java.
    static Pair<Integer, Integer> getWidthHeightForScaleDragShadow(
            Context context, Bitmap shadowImage) {
        float width = shadowImage.getWidth();
        float height = shadowImage.getHeight();
        assert width > 0 && height > 0;

        Resources resources = context.getResources();
        // Calculate the default scaled width / height.
        final float resizeRatio =
                ResourcesCompat.getFloat(resources, R.dimen.drag_shadow_resize_ratio);
        width *= resizeRatio;
        height *= resizeRatio;

        // Scale the image up if it fell short than the min width.
        final int minWidthPx = resources.getDimensionPixelSize(R.dimen.drag_shadow_min_width);
        if (width < minWidthPx) {
            float scaleUpRatio = minWidthPx / width;
            height *= scaleUpRatio;
            width *= scaleUpRatio;
        }

        // Scale the image down if it exceeded the max width / height, while keeping its
        // weight-to-height ratio.
        // Note that this down scale disregard the min width requirement.
        final float maxSizeRatio =
                ResourcesCompat.getFloat(resources, R.dimen.drag_shadow_max_size_to_window_ratio);
        // TODO(https://crbug.com/1297215): Consider switching metrics to the app window.
        final DisplayMetrics metrics = context.getResources().getDisplayMetrics();
        final float maxHeightPx = metrics.heightPixels * maxSizeRatio;
        final float maxWidthPx = metrics.widthPixels * maxSizeRatio;
        if (width > maxWidthPx || height > maxHeightPx) {
            final float downScaleRatio = Math.min(maxHeightPx / height, maxWidthPx / width);
            width *= downScaleRatio;
            height *= downScaleRatio;
        }
        return new Pair<>(Math.round(width), Math.round(height));
    }

    private void reset() {
        mShadowHeight = 0;
        mShadowWidth = 0;
        mIsDragStarted = false;
    }
}
