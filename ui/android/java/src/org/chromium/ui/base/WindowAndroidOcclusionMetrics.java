// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.graphics.Rect;
import android.graphics.Region;
import android.graphics.RegionIterator;
import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Tracks WindowAndroid metrics like occlusion duration by dimension. */
@NullMarked
class WindowAndroidOcclusionMetrics {
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        DimensionBucket.UNINITIALIZED,
        DimensionBucket.BUCKET_0,
        DimensionBucket.BUCKET_1_10,
        DimensionBucket.BUCKET_11_30,
        DimensionBucket.BUCKET_31_50,
        DimensionBucket.BUCKET_51_100,
        DimensionBucket.BUCKET_101_200,
        DimensionBucket.BUCKET_201_PLUS,
        DimensionBucket.BUCKET_FULLY_VISIBLE
    })
    // LINT.IfChange(DimensionBucket)
    @Retention(RetentionPolicy.SOURCE)
    private @interface DimensionBucket {
        int UNINITIALIZED = -1;
        int BUCKET_0 = 0;
        int BUCKET_1_10 = 1;
        int BUCKET_11_30 = 2;
        int BUCKET_31_50 = 3;
        int BUCKET_51_100 = 4;
        int BUCKET_101_200 = 5;
        int BUCKET_201_PLUS = 6;
        int BUCKET_FULLY_VISIBLE = 7;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/histograms.xml:DimensionBucket)

    private @DimensionBucket int mLastMaxMinDimensionBucket = DimensionBucket.UNINITIALIZED;
    private long mLastMaxMinDimensionBucketStartTimeMs;

    /* package */ void onOcclusionStateChanged(boolean isOccluded, @Nullable Region visibleRegion) {
        final @DimensionBucket int currentBucket;

        if (visibleRegion == null) {
            currentBucket =
                    isOccluded ? DimensionBucket.BUCKET_0 : DimensionBucket.BUCKET_FULLY_VISIBLE;
        } else {
            int maxMinDimension = 0;
            RegionIterator iterator = new RegionIterator(visibleRegion);
            Rect rect = new Rect();
            while (iterator.next(rect)) {
                int minDimension = Math.min(rect.width(), rect.height());
                if (minDimension > maxMinDimension) {
                    maxMinDimension = minDimension;
                }
            }
            currentBucket = getDimensionBucket(maxMinDimension);
        }

        if (currentBucket != mLastMaxMinDimensionBucket) {
            long now = SystemClock.uptimeMillis();
            recordMaxMinDimensionHistogram(now);
            mLastMaxMinDimensionBucket = currentBucket;
            mLastMaxMinDimensionBucketStartTimeMs = now;
        }
    }

    /* package */ void onDestroy() {
        recordMaxMinDimensionHistogram(SystemClock.uptimeMillis());
    }

    private @DimensionBucket int getDimensionBucket(int dimension) {
        if (dimension == 0) return DimensionBucket.BUCKET_0;
        if (dimension <= 10) return DimensionBucket.BUCKET_1_10;
        if (dimension <= 30) return DimensionBucket.BUCKET_11_30;
        if (dimension <= 50) return DimensionBucket.BUCKET_31_50;
        if (dimension <= 100) return DimensionBucket.BUCKET_51_100;
        if (dimension <= 200) return DimensionBucket.BUCKET_101_200;
        return DimensionBucket.BUCKET_201_PLUS;
    }

    // LINT.IfChange(DimensionBucketSuffix)
    private String getDimensionBucketSuffix(@DimensionBucket int bucket) {
        switch (bucket) {
            case DimensionBucket.BUCKET_0:
                return "0";
            case DimensionBucket.BUCKET_1_10:
                return "1To10";
            case DimensionBucket.BUCKET_11_30:
                return "11To30";
            case DimensionBucket.BUCKET_31_50:
                return "31To50";
            case DimensionBucket.BUCKET_51_100:
                return "51To100";
            case DimensionBucket.BUCKET_101_200:
                return "101To200";
            case DimensionBucket.BUCKET_201_PLUS:
                return "201Plus";
            case DimensionBucket.BUCKET_FULLY_VISIBLE:
                return "FullyVisible";
            default:
                throw new IllegalArgumentException("Unknown bucket: " + bucket);
        }
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/histograms.xml:DimensionBucket)

    private void recordMaxMinDimensionHistogram(long now) {
        if (mLastMaxMinDimensionBucket != DimensionBucket.UNINITIALIZED) {
            long durationMs = now - mLastMaxMinDimensionBucketStartTimeMs;
            RecordHistogram.recordLongTimesHistogram(
                    "Android.Window.OcclusionExperimental.MaxOfMinVisibleRectsDuration."
                            + getDimensionBucketSuffix(mLastMaxMinDimensionBucket),
                    durationMs);
        }
    }
}
