// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.graphics.Bitmap;
import android.os.Build;
import android.view.View;

/**
 * Delegate to facilitate Drag and Drop operations, for example re-routing the call to {@link
 * #startDragAndDrop(Bitmap, DropDataAndroid).}
 */
public interface DragAndDropDelegate {
    /**
     * General feature switch whether drag and drop is enabled for the current Android OS.
     */
    static boolean isDragAndDropSupportedForOs() {
        // Only enabled on Android O+ to mitigate known issue for drag and drop in Android system.
        // See b/245614280.
        return (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O);
    }

    /** @see View#startDragAndDrop */
    boolean startDragAndDrop(View containerView, Bitmap shadowImage, DropDataAndroid dropData,
            int cursorOffsetX, int cursorOffsetY, int dragObjRectWidth, int dragObjRectHeight);

    /**
     * Set the {@link DragAndDropBrowserDelegate} that will be used to facilitate browser related
     * tasks required for Drag and Drop.
     * @param delegate The {@link DragAndDropBrowserDelegate} that will be used by this class.
     */
    default void setDragAndDropBrowserDelegate(DragAndDropBrowserDelegate delegate) {}
}
