// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.view.MotionEvent;

import androidx.annotation.Nullable;

import org.chromium.base.TraceEvent;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Class with helper methods for MotionEvent.
 */
public class MotionEventUtils {
    /**
     * Returns the time in nanoseconds of the given MotionEvent.
     * It calls the SDK method "getEventTimeNano" via reflection, since this method is hidden. If
     * the reflection fails, the time in milliseconds extended to nanoseconds will be returned.
     */
    public static long getEventTimeNano(MotionEvent event) {
        long timeNs = 0;
        try {
            if (sGetTimeNanoMethod == null) {
                Class<?> cls = Class.forName("android.view.MotionEvent");
                sGetTimeNanoMethod = cls.getMethod("getEventTimeNano");
            }

            timeNs = (long) sGetTimeNanoMethod.invoke(event);
        } catch (IllegalAccessException | NoSuchMethodException | ClassNotFoundException
                | InvocationTargetException e) {
            TraceEvent.instant("MotionEventUtils::getEventTimeNano error", e.toString());
            timeNs = event.getEventTime() * 1_000_000;
        }
        return timeNs;
    }

    /**
     * Returns the time in nanoseconds, but with precision to milliseconds, of the given
     * MotionEvent. There is no SDK method which returns the event time in nanoseconds, so we just
     * extend milliseconds to nanoseconds.
     */
    public static long getHistoricalEventTimeNano(MotionEvent event, int pos) {
        return event.getHistoricalEventTime(pos) * 1_000_000;
    }

    private MotionEventUtils() {}

    @Nullable
    private static Method sGetTimeNanoMethod;
}
