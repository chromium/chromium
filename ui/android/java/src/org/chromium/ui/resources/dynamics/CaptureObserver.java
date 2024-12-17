// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import android.graphics.Canvas;
import android.graphics.Rect;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Observer that's notified before and after a bitmap capture happens. */
@NullMarked
public interface CaptureObserver {
    /**
     * Called before the bitmap capture occurs.
     * @param canvas    The {@link Canvas} that will be drawn to.
     * @param dirtyRect The dirty {@link Rect} or {@code null} if the entire area is being redrawn.
     */
    default void onCaptureStart(Canvas canvas, @Nullable Rect dirtyRect) {}

    /** Called after bitmap capture occurs. */
    default void onCaptureEnd() {}
}
