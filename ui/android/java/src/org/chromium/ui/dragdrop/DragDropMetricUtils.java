// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Define enums for Drag and Drop metrics. This class is not supposed to be instantiated. */
public class DragDropMetricUtils {
    /** Guards this class from being instantiated. */
    private DragDropMetricUtils() {}

    /**
     * Enum used by Android.DragDrop.Tab.Type, which records the drag source and drop target when a
     * tab is dropped successfully. These values are persisted to logs. Entries should not be
     * renumbered and numeric values should never be reused.
     */
    @IntDef({
        DragDropType.TAB_STRIP_TO_TAB_STRIP,
        DragDropType.TAB_STRIP_TO_CONTENT,
        DragDropType.TAB_STRIP_TO_NEW_INSTANCE,
        DragDropType.LINK_TO_NEW_INSTANCE,
        DragDropType.UNKNOWN_TO_NEW_INSTANCE,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DragDropType {
        int TAB_STRIP_TO_TAB_STRIP = 0;
        int TAB_STRIP_TO_CONTENT = 1;
        int TAB_STRIP_TO_NEW_INSTANCE = 2;
        int LINK_TO_NEW_INSTANCE = 3;
        int UNKNOWN_TO_NEW_INSTANCE = 4;
        int NUM_ENTRIES = 5;
    }

    /**
     * On Samsung devices, a new Chrome window could be created via Drag and Drop. This enum
     * reflects the drag source, which is preserved as an intent extra. Before One UI 4.1.1, the
     * intent extras might be stripped. When that happens, the drag source will default to UNKNOWN.
     */
    @IntDef({
        UrlIntentSource.UNKNOWN,
        UrlIntentSource.LINK,
        UrlIntentSource.TAB_IN_STRIP,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface UrlIntentSource {
        int UNKNOWN = 0;
        int LINK = 1;
        int TAB_IN_STRIP = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * Enum used by Android.DragDrop.Tab.FromStrip.Result, which records the tab drag and drop
     * results, including successful drops and failed drops with varies reasons. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({
        DragDropTabResult.SUCCESS,
        DragDropTabResult.IGNORED_TOOLBAR,
        DragDropTabResult.IGNORED_DIFF_MODEL_NOT_SUPPORTED,
        DragDropTabResult.IGNORED_TAB_SWITCHER,
        DragDropTabResult.IGNORED_SAME_INSTANCE,
        DragDropTabResult.ERROR_TAB_NOT_FOUND,
        DragDropTabResult.IGNORED_MAX_INSTANCES,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DragDropTabResult {
        int SUCCESS = 0;
        int IGNORED_TOOLBAR = 1;
        int IGNORED_DIFF_MODEL_NOT_SUPPORTED = 2;
        int IGNORED_TAB_SWITCHER = 3;
        int IGNORED_SAME_INSTANCE = 4;
        int ERROR_TAB_NOT_FOUND = 5;
        int IGNORED_MAX_INSTANCES = 6;
        int NUM_ENTRIES = 7;
    }

    /**
     * Record enumerated histogram Android.DragDrop.Tab.Type.
     *
     * @param dragDropType An enum indicating the drag source and drop target.
     */
    public static void recordTabDragDropType(@DragDropType int dragDropType) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DragDrop.Tab.Type", dragDropType, DragDropType.NUM_ENTRIES);
    }

    /**
     * Record enumerated histogram Android.DragDrop.Tab.FromStrip.Result.
     *
     * @param result An enum indicating the tab drag and drop results, including successful drops
     *     and failed drops with varies reasons.
     */
    public static void recordTabDragDropResult(@DragDropTabResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DragDrop.Tab.FromStrip.Result", result, DragDropTabResult.NUM_ENTRIES);
    }

    /**
     * Record boolean histogram Android.DragDrop.Tab.ReorderStripWithDragDrop.
     *
     * @param leavingStrip Whether the tab drag has left the source strip.
     */
    public static void recordTabReorderStripWithDragDrop(boolean leavingStrip) {
        RecordHistogram.recordBooleanHistogram(
                "Android.DragDrop.Tab.ReorderStripWithDragDrop", leavingStrip);
    }

    /**
     * Record times histogram Android.DragDrop.Tab.Duration.WithinDestStrip.
     *
     * @param duration scrolling on a destination strip.
     */
    public static void recordTabDurationWithinDestStrip(long duration) {
        RecordHistogram.recordMediumTimesHistogram(
                "Android.DragDrop.Tab.Duration.WithinDestStrip", duration);
    }

    /**
     * Record boolean histogram Android.DragDrop.Tab.SourceWindowClosed.
     *
     * @param didCloseWindow Whether a successful tab drag/drop resulted in closing the source
     *     Chrome window.
     */
    public static void recordTabDragDropClosedWindow(boolean didCloseWindow) {
        RecordHistogram.recordBooleanHistogram(
                "Android.DragDrop.Tab.SourceWindowClosed", didCloseWindow);
    }
}
