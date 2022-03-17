// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.ClipData;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build.VERSION_CODES;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.util.Pair;
import android.view.DragEvent;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityManager;
import android.widget.ImageView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.base.compat.ApiHelperForN;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.ui.R;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Drag and drop helper class in charge of building the clip data, wrapping calls to
 * {@link android.view.View#startDragAndDrop}. Also used for mocking out real function calls to
 * Android.
 */
class DragAndDropDelegateImpl implements ViewAndroidDelegate.DragAndDropDelegate, DragStateTracker {
    /**
     * Java Enum of AndroidDragTargetType used for histogram recording for
     * Android.DragDrop.FromWebContent.TargetType. This is used for histograms and should therefore
     * be treated as append-only.
     */
    @IntDef({DragTargetType.INVALID, DragTargetType.TEXT, DragTargetType.IMAGE, DragTargetType.LINK,
            DragTargetType.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    @interface DragTargetType {
        int INVALID = 0;
        int TEXT = 1;
        int IMAGE = 2;
        int LINK = 3;

        int NUM_ENTRIES = 4;
    }

    private int mShadowWidth;
    private int mShadowHeight;
    private boolean mIsDragStarted;

    /** Whether the current drop has happened on top of the view this object tracks.  */
    private boolean mIsDropOnView;

    /** The type of drag target from the view this object tracks. */
    private @DragTargetType int mDragTargetType;

    private float mDragStartXDp;
    private float mDragStartYDp;

    private long mDragStartSystemElapsedTime;

    // Implements ViewAndroidDelegate.DragAndDropDelegate
    /**
     * Wrapper to call {@link android.view.View#startDragAndDrop}.
     * @see ViewAndroidDelegate#startDragAndDrop(Bitmap, DropDataAndroid)
     * */
    @Override
    @RequiresApi(api = VERSION_CODES.N)
    public boolean startDragAndDrop(@NonNull View containerView, @NonNull Bitmap shadowImage,
            @NonNull DropDataAndroid dropData) {
        // Drag and drop is disabled when gesture related a11y service is enabled.
        // See https://crbug.com/1250067.
        AccessibilityManager a11yManager =
                (AccessibilityManager) containerView.getContext().getSystemService(
                        Context.ACCESSIBILITY_SERVICE);
        if (a11yManager.isEnabled() && a11yManager.isTouchExplorationEnabled()) return false;

        ClipData clipdata = buildClipData(dropData);
        if (clipdata == null) {
            return false;
        }

        mIsDragStarted = true;
        mDragStartSystemElapsedTime = SystemClock.elapsedRealtime();
        mDragTargetType = getDragTargetType(dropData);
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
        // Only tracks drag event that started from the #startDragAndDrop.
        if (!mIsDragStarted) return false;

        switch (dragEvent.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                onDragStarted(dragEvent);
                break;
            case DragEvent.ACTION_DROP:
                onDrop(dragEvent);
                break;
            case DragEvent.ACTION_DRAG_ENDED:
                onDragEnd(dragEvent);
                reset();
                break;
            default:
                // No action needed for other types of drag actions.
                break;
        }
        // Return false so this listener does not consume the drag event of the view it listened to.
        return false;
    }

    protected ClipData buildClipData(DropDataAndroid dropData) {
        @DragTargetType
        int type = getDragTargetType(dropData);
        switch (type) {
            case DragTargetType.TEXT:
                return ClipData.newPlainText(null, dropData.text);
            case DragTargetType.IMAGE:
                Uri uri = DropDataContentProvider.cache(
                        dropData.imageContent, dropData.imageContentExtension);
                return ClipData.newUri(
                        ContextUtils.getApplicationContext().getContentResolver(), null, uri);
            case DragTargetType.LINK:
                // TODO(https://crbug.com/1298308): Handle image link dragging.
                return ClipData.newPlainText(null, getTextForLinkData(dropData));
            case DragTargetType.INVALID:
                return null;
            case DragTargetType.NUM_ENTRIES:
            default:
                assert false : "Should not be reached!";
                return null;
        }
    }

    @RequiresApi(api = VERSION_CODES.N)
    protected int buildFlags(DropDataAndroid dropData) {
        if (dropData.isPlainText()) {
            return View.DRAG_FLAG_GLOBAL;
        } else if (dropData.hasImage()) {
            return View.DRAG_FLAG_GLOBAL | View.DRAG_FLAG_GLOBAL_URI_READ;
        } else if (dropData.hasLink()) {
            return View.DRAG_FLAG_GLOBAL;
        } else {
            return 0;
        }
    }

    protected View.DragShadowBuilder createDragShadowBuilder(
            Context context, Bitmap shadowImage, boolean isImage) {
        ImageView imageView = new ImageView(context);
        if (isImage) {
            // If drag shadow image is an 1*1 image, it is not considered as a valid drag shadow.
            // In such cases, use a globe icon as placeholder instead. See
            // https://crbug.com/1304433.
            if (shadowImage.getHeight() <= 1 && shadowImage.getWidth() <= 1) {
                Drawable globeIcon =
                        AppCompatResources.getDrawable(context, R.drawable.ic_globe_24dp);
                assert globeIcon != null;

                final int minSize = getDragShadowMinWidth(context.getResources());
                mShadowWidth = minSize;
                mShadowHeight = minSize;
                imageView.setLayoutParams(new ViewGroup.LayoutParams(minSize, minSize));
                imageView.setScaleType(ImageView.ScaleType.FIT_CENTER);
                imageView.setImageDrawable(globeIcon);
            } else {
                Pair<Integer, Integer> widthHeight =
                        getWidthHeightForScaleDragShadow(context, shadowImage);
                mShadowWidth = widthHeight.first;
                mShadowHeight = widthHeight.second;
                imageView.setImageBitmap(shadowImage);
            }
        } else {
            mShadowWidth = shadowImage.getWidth();
            mShadowHeight = shadowImage.getHeight();
            imageView.setImageBitmap(shadowImage);
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
        final int minWidthPx = getDragShadowMinWidth(resources);
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

    private void onDragStarted(DragEvent dragStartEvent) {
        mDragStartXDp = dragStartEvent.getX();
        mDragStartYDp = dragStartEvent.getY();
    }

    private void onDrop(DragEvent dropEvent) {
        mIsDropOnView = true;

        final int dropDistance = Math.round(MathUtils.distance(
                mDragStartXDp, mDragStartYDp, dropEvent.getX(), dropEvent.getY()));
        RecordHistogram.recordExactLinearHistogram(
                "Android.DragDrop.FromWebContent.DropInWebContent.DistanceDip", dropDistance, 51);

        long dropDuration = SystemClock.elapsedRealtime() - mDragStartSystemElapsedTime;
        RecordHistogram.recordMediumTimesHistogram(
                "Android.DragDrop.FromWebContent.DropInWebContent.Duration", dropDuration);
    }

    private void onDragEnd(DragEvent dragEndEvent) {
        boolean dragResult = dragEndEvent.getResult();

        // Only record metrics when drop does not happen for ContentView.
        if (!mIsDropOnView) {
            assert mDragStartSystemElapsedTime > 0;
            assert mDragTargetType != DragTargetType.INVALID;
            long dragDuration = SystemClock.elapsedRealtime() - mDragStartSystemElapsedTime;
            recordDragDurationAndResult(dragDuration, dragResult);
            recordDragTargetType(mDragTargetType);
        }

        DropDataContentProvider.onDragEnd(!mIsDropOnView && dragResult);
    }

    /**
     * Return the {@link DragTargetType} based on the content of DropDataAndroid. The result will
     * bias plain text > image > link.
     * TODO(https://crbug.com/1299994): Manage the ClipData bias with EventForwarder in one place.
     */
    static @DragTargetType int getDragTargetType(DropDataAndroid dropDataAndroid) {
        if (dropDataAndroid.isPlainText()) {
            return DragTargetType.TEXT;
        } else if (dropDataAndroid.hasImage()) {
            return DragTargetType.IMAGE;
        } else if (dropDataAndroid.hasLink()) {
            return DragTargetType.LINK;
        } else {
            return DragTargetType.INVALID;
        }
    }

    private static int getDragShadowMinWidth(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.drag_shadow_min_width);
    }

    /**
     * Return the text to be dropped when {@link DropDataAndroid} contains a link.
     */
    static String getTextForLinkData(DropDataAndroid dropData) {
        assert dropData.hasLink();
        if (TextUtils.isEmpty(dropData.text)) return dropData.gurl.getSpec();
        return dropData.text + "\n" + dropData.gurl.getSpec();
    }

    private void reset() {
        mShadowHeight = 0;
        mShadowWidth = 0;
        mDragTargetType = DragTargetType.INVALID;
        mIsDragStarted = false;
        mIsDropOnView = false;
        mDragStartSystemElapsedTime = -1;
    }

    private void recordDragTargetType(@DragTargetType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DragDrop.FromWebContent.TargetType", type, DragTargetType.NUM_ENTRIES);
    }

    private void recordDragDurationAndResult(long duration, boolean result) {
        String histogramPrefix = "Android.DragDrop.FromWebContent.Duration.";
        String suffix = result ? "Success" : "Canceled";
        RecordHistogram.recordMediumTimesHistogram(histogramPrefix + suffix, duration);
    }
}
