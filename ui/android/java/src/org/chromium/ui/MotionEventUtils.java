// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.os.Build;
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
     *
     * This method exists as a utility pre API 34 (Android U) there was no public method to get
     * nanoseconds. So we call the hidden SDK method "getEventTimeNano" via reflection. If the
     * reflection fails, the time in milliseconds extended to nanoseconds will be returned.
     */
    public static long getEventTimeNanos(MotionEvent event) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            return event.getEventTimeNanos();
        }
        if (sFailedReflection) {
            return event.getEventTime() * 1_000_000;
        }
        long timeNs = 0;
        // We are calling a method that was set as maxSDK=P, there are strictmode violations but
        // suppressing it with StrictModeContext.allowAllVmPolicies() (or event just NonSDKUsage
        // suppression) results in a binder call which takes 1.2ms at the median. See
        // crbug/1454299#c21. So we just allow the violation to occur on Android P to Android U.
        try {
            if (sGetTimeNanoMethod == null) {
                sGetTimeNanoMethod = MotionEvent.class.getMethod("getEventTimeNano");
            }
            timeNs = (long) sGetTimeNanoMethod.invoke(event);
        } catch (IllegalAccessException | NoSuchMethodException | InvocationTargetException e) {
            TraceEvent.instant("MotionEventUtils::getEventTimeNano error", e.toString());
            sFailedReflection = true;
            timeNs = event.getEventTime() * 1_000_000;
        }
        return timeNs;
    }

    /**
     * Returns the time in nanoseconds, but with precision to milliseconds, of the given
     * MotionEvent. There is no SDK method which returns the event time in nanoseconds, pre Android
     * API 34 (Android U) so we just extend milliseconds to nanoseconds in that case.
     */
    public static long getHistoricalEventTimeNanos(MotionEvent event, int pos) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            return event.getHistoricalEventTimeNanos(pos);
        }
        return event.getHistoricalEventTime(pos) * 1_000_000;
    }

    private MotionEventUtils() {}

    @Nullable
    private static Method sGetTimeNanoMethod;
    private static boolean sFailedReflection;
}
