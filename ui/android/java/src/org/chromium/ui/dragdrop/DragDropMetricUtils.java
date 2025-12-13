// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Define enums for Drag and Drop metrics. This class is not supposed to be instantiated. */
@NullMarked
public class DragDropMetricUtils {
    public static String HISTOGRAM_DRAG_DROP_TAB_TYPE = "Android.DragDrop.Tab.Type";

    /** Guards this class from being instantiated. */
    private DragDropMetricUtils() {}

    /**
     * Enum used by Android.DragDrop.Tab.Type and Android.DragDrop.TabGroup.Type, which records the
     * drag source and drop target when a tab is dropped successfully. These values are persisted to
     * logs. Entries should not be renumbered and numeric values should never be reused.
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
        UrlIntentSource.TAB_GROUP_IN_STRIP,
        UrlIntentSource.MULTI_TAB_IN_STRIP,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface UrlIntentSource {
        int UNKNOWN = 0;
        int LINK = 1;
        int TAB_IN_STRIP = 2;
        int TAB_GROUP_IN_STRIP = 3;
        int MULTI_TAB_IN_STRIP = 4;
        int NUM_ENTRIES = 5;
    }

    /**
     * Enum used by Android.DragDrop.Tab*.FromStrip.Result.* which records the drag and drop
     * results, including successful drops and failed drops with varies reasons. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({
        DragDropResult.SUCCESS,
        DragDropResult.IGNORED_TOOLBAR,
        DragDropResult.IGNORED_DIFF_MODEL_NOT_SUPPORTED,
        DragDropResult.IGNORED_TAB_SWITCHER,
        DragDropResult.IGNORED_SAME_INSTANCE,
        DragDropResult.ERROR_CONTENT_NOT_FOUND,
        DragDropResult.IGNORED_MAX_INSTANCES,
        DragDropResult.IGNORED_MHTML_TAB,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DragDropResult {
        int SUCCESS = 0;
        int IGNORED_TOOLBAR = 1;
        int IGNORED_DIFF_MODEL_NOT_SUPPORTED = 2;
        int IGNORED_TAB_SWITCHER = 3;
        int IGNORED_SAME_INSTANCE = 4;
        int ERROR_CONTENT_NOT_FOUND = 5;
        int IGNORED_MAX_INSTANCES = 6;
        int IGNORED_MHTML_TAB = 7;
        int NUM_ENTRIES = 8;
    }

    /**
     * Record enumerated histogram Android.DragDrop.Tab.Type and Android.DragDrop.TabGroup.Type for
     * dragged tab or tab group data.
     *
     * @param dragDropType An enum indicating the drag source and drop target.
     * @param isInDesktopWindow Whether the app is running in a desktop window.
     * @param isTabGroup True if the dragged item is a tab group.
     * @param isMultiTab True if the dragged item is part of a multi-tab operation.
     */
    public static void recordDragDropType(
            @DragDropType int dragDropType,
            boolean isInDesktopWindow,
            boolean isTabGroup,
            boolean isMultiTab) {
        String dragDropItemType = getDragDropItemType(isTabGroup, isMultiTab);
        String histogram = String.format("Android.DragDrop.%s.Type", dragDropItemType);
        RecordHistogram.recordEnumeratedHistogram(
                histogram, dragDropType, DragDropType.NUM_ENTRIES);
        if (isInDesktopWindow) {
            RecordHistogram.recordEnumeratedHistogram(
                    histogram + ".DesktopWindow", dragDropType, DragDropType.NUM_ENTRIES);
        }
    }

    /**
     * Record enumerated histograms Android.DragDrop.Tab.FromStrip.Result and
     * Android.DragDrop.TabGroup.FromStrip.Result.
     *
     * @param result An enum indicating the tab or tab group drag and drop results, including
     *     successful drops and failed drops with varies reasons.
     * @param isInDesktopWindow Whether the app is running in a desktop window.
     * @param isTabGroup True if the dragged item is a tab group; otherwise, it is assumed to be a
     *     single tab.
     * @param isMultiTab True if the dragged item is part of a multi-tab operation.
     */
    public static void recordDragDropResult(
            @DragDropResult int result,
            boolean isInDesktopWindow,
            boolean isTabGroup,
            boolean isMultiTab) {
        String dragDropItemType = getDragDropItemType(isTabGroup, isMultiTab);
        String histogram = String.format("Android.DragDrop.%s.FromStrip.Result", dragDropItemType);
        RecordHistogram.recordEnumeratedHistogram(histogram, result, DragDropResult.NUM_ENTRIES);
        if (isInDesktopWindow) {
            RecordHistogram.recordEnumeratedHistogram(
                    histogram + ".DesktopWindow", result, DragDropResult.NUM_ENTRIES);
        }
    }

    /**
     * Record boolean histogram Android.DragDrop.Tab.ReorderStripWithDragDrop and
     * Android.DragDrop.TabGroup.ReorderStripWithDragDrop.
     *
     * @param leavingStrip Whether the tab drag has left the source strip.
     * @param isTabGroup True if the dragged item is a tab group; otherwise, it is assumed to be a
     *     single tab.
     * @param isMultiTab True if the dragged item is part of a multi-tab operation.
     */
    public static void recordReorderStripWithDragDrop(
            boolean leavingStrip, boolean isTabGroup, boolean isMultiTab) {
        String dragDropItemType = getDragDropItemType(isTabGroup, isMultiTab);
        String histogram =
                String.format("Android.DragDrop.%s.ReorderStripWithDragDrop", dragDropItemType);
        RecordHistogram.recordBooleanHistogram(histogram, leavingStrip);
    }

    /**
     * Record boolean histogram Android.DragDrop.Tab.SourceWindowClosed and
     * Android.DragDrop.TabGroup.SourceWindowClosed.
     *
     * @param didCloseWindow Whether a successful tab drag/drop resulted in closing the source
     *     Chrome window.
     * @param isTabGroup True if the dragged item is a tab group; otherwise, it is assumed to be a
     *     single tab.
     * @param isMultiTab True if the dragged item is part of a multi-tab operation.
     */
    public static void recordDragDropClosedWindow(
            boolean didCloseWindow, boolean isTabGroup, boolean isMultiTab) {
        String dragDropItemType = getDragDropItemType(isTabGroup, isMultiTab);
        String histogram =
                String.format("Android.DragDrop.%s.SourceWindowClosed", dragDropItemType);
        RecordHistogram.recordBooleanHistogram(histogram, didCloseWindow);
    }

    private static String getDragDropItemType(boolean isTabGroup, boolean isMultiTab) {
        if (isMultiTab) {
            return "MultiTab";
        } else if (isTabGroup) {
            return "TabGroup";
        } else {
            return "Tab";
        }
    }
}
