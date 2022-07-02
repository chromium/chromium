// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.graphics.Bitmap;
import android.view.View;

/**
 * Delegate to facilitate Drag and Drop operations, for example re-routing the call to {@link
 * #startDragAndDrop(Bitmap, DropDataAndroid).}
 */
public interface DragAndDropDelegate {
    /** @see View#startDragAndDrop */
    boolean startDragAndDrop(View containerView, Bitmap shadowImage, DropDataAndroid dropData);

    /**
     * Set the {@link DragAndDropBrowserDelegate} that will be used to facilitate browser related
     * tasks required for Drag and Drop.
     * @param delegate The {@link DragAndDropBrowserDelegate} that will be used by this class.
     */
    default void setDragAndDropBrowserDelegate(DragAndDropBrowserDelegate delegate) {}
}
