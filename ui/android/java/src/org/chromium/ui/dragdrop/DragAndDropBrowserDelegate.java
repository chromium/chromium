// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.content.ClipData;
import android.content.Intent;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;

import org.chromium.ui.dragdrop.DragDropMetricUtils.UrlIntentSource;

/** Delegate for browser related functions used by Drag and Drop. */
public interface DragAndDropBrowserDelegate {
    /** Get whether to support the image drop into Chrome. */
    boolean getSupportDropInChrome();

    /** Get whether to support the image drag shadow animation. */
    boolean getSupportAnimatedImageDragShadow();

    /** Request DragAndDropPermissions. */
    DragAndDropPermissions getDragAndDropPermissions(DragEvent dropEvent);

    /** Create an intent from a dragged URL. */
    Intent createUrlIntent(String urlString, @UrlIntentSource int intentSrc);

    /** Build clip data for drag. */
    ClipData buildClipData(DropDataAndroid dropData);

    /**
     * Update the flags used for drag and drop.
     *
     * @param originalFlags The original flags by {@link DragAndDropDelegateImpl#buildFlags}.
     * @param dropData {@link DropDataAndroid} used during drag/drop.
     * @return The flags to be used by {@link android.view.View#startDragAndDrop}.
     */
    int buildFlags(int originalFlags, DropDataAndroid dropData);
}
