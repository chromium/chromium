// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * @hide
 */
@IntDef({ScrollNotificationType.DIRECTION_CHANGED_UP,
        ScrollNotificationType.DIRECTION_CHANGED_DOWN})
@Retention(RetentionPolicy.SOURCE)
@interface ScrollNotificationType {
    /**
     * This is the direction toward vertical scroll offset 0. Note direction change notification
     * is sent on direction change. If there are two consecutive scrolls in the same direction,
     * the second scroll will not generate a direction change notification. Also the notification
     * is sent as a result of scroll change; this means for touch scrolls, this is sent (if there
     * is a direction change) on the first touch move, not touch down.
     */
    int DIRECTION_CHANGED_UP =
            org.chromium.weblayer_private.interfaces.ScrollNotificationType.DIRECTION_CHANGED_UP;

    /**
     * This is the direction away from vertical scroll offset 0. See notes on DIRECTION_CHANGED_UP.
     */
    int DIRECTION_CHANGED_DOWN =
            org.chromium.weblayer_private.interfaces.ScrollNotificationType.DIRECTION_CHANGED_DOWN;
}
