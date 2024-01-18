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
     * Record enumerated histogram Android.DragDrop.Tab.Type.
     *
     * @param dragDropType An enum indicating the drag source and drop target.
     */
    public static void recordTabDragDropType(@DragDropType int dragDropType) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.DragDrop.Tab.Type", dragDropType, DragDropType.NUM_ENTRIES);
    }
}
