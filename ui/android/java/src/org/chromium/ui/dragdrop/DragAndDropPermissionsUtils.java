// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.app.Activity;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Utilities for requesting {@link DragAndDropPermissions} for drop events. */
@NullMarked
public final class DragAndDropPermissionsUtils {
    private static final String TAG = "DragAndDropPerms";

    private DragAndDropPermissionsUtils() {}

    /**
     * Requests the URI permissions granted by the source of a drop and returns null when no
     * permissions were granted or when a SecurityException is thrown.
     *
     * @param activity The activity that received the drop.
     * @param event The {@link DragEvent#ACTION_DROP} event.
     * @return The granted permissions, or null if no permissions were granted.
     */
    public static @Nullable DragAndDropPermissions requestPermissions(
            Activity activity, DragEvent event) {
        try {
            return activity.requestDragAndDropPermissions(event);
        } catch (SecurityException e) {
            Log.w(TAG, "Drag source cannot grant URI permissions for drop", e);
            return null;
        }
    }
}
