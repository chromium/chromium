// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.graphics.Bitmap;
import android.view.View;

/**
 * Delegate to re-route the call to {@link #startDragAndDrop(Bitmap, DropDataAndroid).}
 */
public interface DragAndDropDelegate {
    /** @see View#startDragAndDrop */
    boolean startDragAndDrop(View containerView, Bitmap shadowImage, DropDataAndroid dropData);
}
