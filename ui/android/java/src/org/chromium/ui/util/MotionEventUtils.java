// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import android.os.Build;
import android.view.InputDevice;
import android.view.MotionEvent;

import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Class with helper methods for MotionEvent.
 *
 * <p>Not thread safe.
 */
@NullMarked
public class MotionEventUtils {
    /**
     * {@link MotionEvent} button constant: no button.
     *
     * <p>This complements the {@code MotionEvent.BUTTON_*} definitions in Android framework.
     * Currently {@link MotionEvent#getButtonState()} returns 0 when no button is pressed, but the
     * framework doesn't define a constant for this.
     *
     * @see MotionEvent#getButtonState()
     */
    public static final int MOTION_EVENT_BUTTON_NONE = 0;

    private static @Nullable Method sGetTimeNanoMethod;
    private static boolean sFailedReflection;

    /**
     * Returns the time in nanoseconds of the given MotionEvent.
     *
     * <p>This method exists as a utility pre API 34 (Android U) there was no public method to get
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

    private static long approximateNanosFromEvent(MotionEvent event, int pos) {
        return event.getHistoricalEventTime(pos) * 1_000_000;
    }

    private static boolean sFailedDoubleReflection;
    private static @Nullable Method sGetHistoricalEventTimeNanoMethod;

    /**
     * Returns the time in nanoseconds, but with precision to milliseconds, of the given
     * MotionEvent. There is no SDK method which returns the event time in nanoseconds, pre Android
     * API 34 (Android U) so we just extend milliseconds to nanoseconds in that case.
     */
    public static long getHistoricalEventTimeNanos(MotionEvent event, int pos) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            return event.getHistoricalEventTimeNanos(pos);
        }
        if (sFailedDoubleReflection) {
            return approximateNanosFromEvent(event, pos);
        }
        try {
            // Before Android U the method was declared in core/java/android/view/MotionEvent.java
            // as "public final long getHistoricalEventTimeNano(int pos)". It was added in 2012. All
            // Android releases supported by Chrome should have it.
            if (sGetHistoricalEventTimeNanoMethod == null) {
                Method getDeclaredMethod =
                        Class.class.getDeclaredMethod(
                                "getDeclaredMethod", String.class, Class[].class);
                Class[] cArg = new Class[1];
                cArg[0] = int.class;
                Method method =
                        (Method)
                                getDeclaredMethod.invoke(
                                        event.getClass(), "getHistoricalEventTimeNano", cArg);
                method.setAccessible(true);
                sGetHistoricalEventTimeNanoMethod = method;
            }
            return (long) sGetHistoricalEventTimeNanoMethod.invoke(event, pos);
        } catch (Exception e) {
            TraceEvent.instant("MotionEventUtils::getHistoricalEventTimeNanos error", e.toString());
            sFailedDoubleReflection = true;
            return approximateNanosFromEvent(event, pos);
        }
    }

    /** Returns true if a {@link MotionEvent} is detected to be a mouse event. */
    public static boolean isMouseEvent(MotionEvent event) {
        return isMouseEvent(event.getSource(), event.getToolType(0));
    }

    /**
     * Same as {@link #isMouseEvent(MotionEvent)}, but this method accepts individual properties of
     * a {@link MotionEvent}.
     *
     * <p>This is useful when you only have information about a {@link MotionEvent}, but not a
     * reference to the {@link MotionEvent} itself, such as when you need to keep motion information
     * after a {@link MotionEvent} gets recycled.
     *
     * @param source same as {@link MotionEvent#getSource()}
     * @param toolType same as {@code MotionEvent#getToolType(0)} (the first pointer that is down).
     * @return true if the parameters match a mouse motion event.
     * @see MotionEvent#recycle()
     */
    public static boolean isMouseEvent(int source, int toolType) {
        return (source & InputDevice.SOURCE_CLASS_POINTER) != 0
                && toolType == MotionEvent.TOOL_TYPE_MOUSE;
    }

    /**
     * Returns true if a {@link MotionEvent} is detected to be a trackpad event. Note that {@link
     * MotionEvent.TOOL_TYPE_FINGER} is used here along with {@link InputDevice.SOURCE_MOUSE}
     * instead of {@link InputDevice.SOURCE_TOUCHPAD} because {@link InputDevice.SOURCE_TOUCHPAD} is
     * used when an app captures the touchpad meaning that it gets access to the raw finger
     * locations, dimensions etc. reported by the touchpad rather than those being used for pointer
     * movements and gestures.
     */
    public static boolean isTrackpadEvent(MotionEvent event) {
        return event.isFromSource(InputDevice.SOURCE_MOUSE)
                && event.getToolType(0) == MotionEvent.TOOL_TYPE_FINGER;
    }

    /** Checks if the motion event was generated by touch (buttons == 0) OR via a primary button. */
    public static boolean isTouchOrPrimaryButton(int buttons) {
        return buttons == MOTION_EVENT_BUTTON_NONE || isPrimaryButton(buttons);
    }

    /**
     * Checks if the motion event was generated via a primary button (from mouse, trackpad etc).
     * This button constant is not set in response to simple touches with a finger or stylus tip.
     * The user must actually push a button.
     */
    public static boolean isPrimaryButton(int buttons) {
        return (buttons & MotionEvent.BUTTON_PRIMARY) != 0;
    }

    /** Checks if the motion event was generated by a tertiary button (middle mouse button). */
    public static boolean isTertiaryButton(int buttons) {
        return (buttons & MotionEvent.BUTTON_TERTIARY) != 0;
    }

    /** Checks if the motion event was generated by a secondary button (middle mouse button). */
    public static boolean isSecondaryClick(int buttons) {
        return (buttons & MotionEvent.BUTTON_SECONDARY) != 0;
    }
}
