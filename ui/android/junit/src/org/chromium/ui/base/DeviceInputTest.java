// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static android.view.InputDevice.KEYBOARD_TYPE_ALPHABETIC;
import static android.view.InputDevice.KEYBOARD_TYPE_NONE;
import static android.view.InputDevice.KEYBOARD_TYPE_NON_ALPHABETIC;
import static android.view.InputDevice.SOURCE_KEYBOARD;
import static android.view.InputDevice.SOURCE_MOUSE;
import static android.view.InputDevice.SOURCE_TOUCHPAD;

import static com.google.common.truth.Truth.assertThat;

import android.util.SparseArray;
import android.view.InputDevice;
import android.view.MotionEvent;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;
import org.robolectric.shadow.api.Shadow;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.function.Consumer;

/** Tests for {@link DeviceInput}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        shadows = {
            DeviceInputTest.ShadowInputDevice.class,
            DeviceInputTest.ShadowInputDevice.ShadowMotionRange.class
        })
public class DeviceInputTest {

    @After
    public void tearDown() {
        ShadowInputDevice.reset();
    }

    @Test
    @SmallTest
    public void testNotifyNullInputDeviceAdded() {
        int nextDeviceId = 1;

        // Attach a physical alphabetic keyboard.
        ShadowInputDevice.attach(nextDeviceId++)
                .setKeyboardType(KEYBOARD_TYPE_ALPHABETIC)
                .setSources(SOURCE_KEYBOARD);

        // Attach a physical precision pointer.
        ShadowInputDevice.attach(nextDeviceId++).setSources(SOURCE_MOUSE);

        // Verify initial state.
        Assert.assertTrue(DeviceInput.supportsAlphabeticKeyboard());
        Assert.assertTrue(DeviceInput.supportsPrecisionPointer());

        // Case: Notify addition of a non-existent device.
        int nonExistentDeviceId = -1;
        Assert.assertNull(InputDevice.getDevice(nonExistentDeviceId));
        ShadowInputDevice.notifyInputDeviceAdded(nonExistentDeviceId);
        Assert.assertTrue(DeviceInput.supportsAlphabeticKeyboard());
        Assert.assertTrue(DeviceInput.supportsPrecisionPointer());
    }

    @Test
    @SmallTest
    public void testNotifyNullInputDeviceChanged() {
        int nextDeviceId = 1;

        // Attach a physical alphabetic keyboard.
        ShadowInputDevice alphabeticKeyboard =
                ShadowInputDevice.attach(nextDeviceId++)
                        .setKeyboardType(KEYBOARD_TYPE_ALPHABETIC)
                        .setSources(SOURCE_KEYBOARD);

        // Attach a physical precision pointer.
        ShadowInputDevice precisionPointer =
                ShadowInputDevice.attach(nextDeviceId++).setSources(SOURCE_MOUSE);

        // Verify initial state.
        Assert.assertTrue(DeviceInput.supportsAlphabeticKeyboard());
        Assert.assertTrue(DeviceInput.supportsPrecisionPointer());

        // Case: Notify change of a non-existent device.
        int nonExistentDeviceId = -1;
        Assert.assertNull(InputDevice.getDevice(nonExistentDeviceId));
        ShadowInputDevice.notifyInputDeviceChanged(nonExistentDeviceId);
        Assert.assertTrue(DeviceInput.supportsAlphabeticKeyboard());
        Assert.assertTrue(DeviceInput.supportsPrecisionPointer());

        // Case: Notify change of a detached physical alphabetic keyboard.
        ShadowInputDevice.detach(alphabeticKeyboard.getId(), /* notifyAsync= */ true);
        Assert.assertNull(InputDevice.getDevice(alphabeticKeyboard.getId()));
        Assert.assertTrue(DeviceInput.supportsAlphabeticKeyboard());
        Assert.assertTrue(DeviceInput.supportsPrecisionPointer());
        ShadowInputDevice.notifyInputDeviceChanged(alphabeticKeyboard.getId());
        Assert.assertFalse(DeviceInput.supportsAlphabeticKeyboard());
        Assert.assertTrue(DeviceInput.supportsPrecisionPointer());

        // Case: Notify change of a detached physical precision pointer.
        ShadowInputDevice.detach(precisionPointer.getId(), /* notifyAsync= */ true);
        Assert.assertNull(InputDevice.getDevice(precisionPointer.getId()));
        Assert.assertFalse(DeviceInput.supportsAlphabeticKeyboard());
        Assert.assertTrue(DeviceInput.supportsPrecisionPointer());
        ShadowInputDevice.notifyInputDeviceChanged(precisionPointer.getId());
        Assert.assertFalse(DeviceInput.supportsAlphabeticKeyboard());
        Assert.assertFalse(DeviceInput.supportsPrecisionPointer());
    }

    @Test
    @SmallTest
    public void testNotifyNullInputDeviceRemoved() {
        int nextDeviceId = 1;

        // Attach a physical alphabetic keyboard.
        ShadowInputDevice.attach(nextDeviceId++)
                .setKeyboardType(KEYBOARD_TYPE_ALPHABETIC)
                .setSources(SOURCE_KEYBOARD);

        // Attach a physical precision pointer.
        ShadowInputDevice.attach(nextDeviceId++).setSources(SOURCE_MOUSE);

        // Verify initial state.
        Assert.assertTrue(DeviceInput.supportsAlphabeticKeyboard());
        Assert.assertTrue(DeviceInput.supportsPrecisionPointer());

        // Case: Notify removal of a non-existent device.
        int nonExistentDeviceId = -1;
        Assert.assertNull(InputDevice.getDevice(nonExistentDeviceId));
        ShadowInputDevice.notifyInputDeviceRemoved(nonExistentDeviceId);
        Assert.assertTrue(DeviceInput.supportsAlphabeticKeyboard());
        Assert.assertTrue(DeviceInput.supportsPrecisionPointer());
    }

    @Test
    @SmallTest
    public void testSupportsAlphabeticKeyboard() {
        int nextDeviceId = 1;

        // Attach non-keyboard.
        ShadowInputDevice.attach(nextDeviceId++)
                .setKeyboardType(KEYBOARD_TYPE_NONE)
                .setSources(SOURCE_MOUSE);

        // Attach non-alphabetic keyboard.
        ShadowInputDevice.attach(nextDeviceId++)
                .setKeyboardType(KEYBOARD_TYPE_NON_ALPHABETIC)
                .setSources(SOURCE_KEYBOARD);

        // Attach non-physical alphabetic keyboard.
        ShadowInputDevice alphabeticKeyboard =
                ShadowInputDevice.attach(nextDeviceId++)
                        .setIsVirtual(true)
                        .setKeyboardType(KEYBOARD_TYPE_ALPHABETIC)
                        .setSources(SOURCE_KEYBOARD);

        // Case: No physical alphabetic keyboards attached.
        Assert.assertFalse(DeviceInput.supportsAlphabeticKeyboard());

        // Case: A non-physical alphabetic keyboard was updated to become physical (contrived).
        alphabeticKeyboard.setIsVirtual(false);
        Assert.assertTrue(DeviceInput.supportsAlphabeticKeyboard());

        // Case: A physical alphabetic keyboard was detached.
        ShadowInputDevice.detach(alphabeticKeyboard.getId());
        Assert.assertFalse(DeviceInput.supportsAlphabeticKeyboard());

        // Case: A physical alphabetic keyboard was attached.
        ShadowInputDevice.attach(nextDeviceId++)
                .setKeyboardType(KEYBOARD_TYPE_ALPHABETIC)
                .setSources(SOURCE_KEYBOARD);
        Assert.assertTrue(DeviceInput.supportsAlphabeticKeyboard());
    }

    @Test
    @SmallTest
    public void testSupportsPrecisionPointer() {
        int nextDeviceId = 1;

        // Attach non-precision pointer.
        ShadowInputDevice.attach(nextDeviceId++)
                .setKeyboardType(KEYBOARD_TYPE_ALPHABETIC)
                .setSources(SOURCE_KEYBOARD);

        // Attach precision pointer.
        ShadowInputDevice precisionPointer =
                ShadowInputDevice.attach(nextDeviceId++)
                        .setIsVirtual(true)
                        .setSources(SOURCE_MOUSE);

        // Case: No physical precision pointer attached.
        Assert.assertFalse(DeviceInput.supportsPrecisionPointer());

        // Case: A non-physical precision pointer was updated to become physical (contrived).
        precisionPointer.setIsVirtual(false);
        Assert.assertTrue(DeviceInput.supportsPrecisionPointer());

        // Case: A physical precision pointer was detached.
        ShadowInputDevice.detach(precisionPointer.getId());
        Assert.assertFalse(DeviceInput.supportsPrecisionPointer());

        // Case: A physical precision pointer was attached.
        ShadowInputDevice.attach(nextDeviceId++).setSources(SOURCE_MOUSE);
        Assert.assertTrue(DeviceInput.supportsPrecisionPointer());
    }

    @Test
    @SmallTest
    public void testSupportsMotionRanges() {
        int deviceId = 1;

        ShadowInputDevice.attach(deviceId)
                .setSources(SOURCE_TOUCHPAD)
                .addMotionRange(MotionEvent.AXIS_X, SOURCE_TOUCHPAD, 5)
                .addMotionRange(MotionEvent.AXIS_Y, SOURCE_TOUCHPAD, 10);

        // Verify initial state.
        Assert.assertEquals(
                DeviceInput.getTouchpadXAxisMotionRange(deviceId).getResolution(), 5, 0.01);
        Assert.assertEquals(
                DeviceInput.getTouchpadYAxisMotionRange(deviceId).getResolution(), 10, 0.01);
    }

    @Test
    @SmallTest
    public void testSupportsMotionRangesWrongSource() {
        int deviceId = 1;

        ShadowInputDevice.attach(deviceId)
                .setSources(SOURCE_TOUCHPAD)
                .addMotionRange(MotionEvent.AXIS_X, SOURCE_MOUSE, 5)
                .addMotionRange(MotionEvent.AXIS_Y, SOURCE_MOUSE, 10);

        // Verify initial state.
        Assert.assertNull(DeviceInput.getTouchpadXAxisMotionRange(deviceId));
        Assert.assertNull(DeviceInput.getTouchpadYAxisMotionRange(deviceId));
    }

    @Test
    @SmallTest
    public void testSupportsMotionRangesWrongDeviceId() {
        int deviceId = 1;
        int nonExistentDeviceId = 2;

        ShadowInputDevice.attach(deviceId)
                .setSources(SOURCE_TOUCHPAD)
                .addMotionRange(MotionEvent.AXIS_X, SOURCE_TOUCHPAD, 5)
                .addMotionRange(MotionEvent.AXIS_Y, SOURCE_TOUCHPAD, 10);

        // Verify initial state.
        Assert.assertNull(DeviceInput.getTouchpadXAxisMotionRange(nonExistentDeviceId));
        Assert.assertNull(DeviceInput.getTouchpadYAxisMotionRange(nonExistentDeviceId));
    }

    @Implements(InputDevice.class)
    public static class ShadowInputDevice extends org.robolectric.shadows.ShadowInputDevice {

        private static final SparseArray<InputDevice> sDevicesById = new SparseArray<>();

        private int mId;
        private boolean mIsVirtual;
        private int mKeyboardType;
        private int mSources;
        private final ArrayList<InputDevice.MotionRange> mMotionRanges = new ArrayList<>();

        public static ShadowInputDevice attach(int deviceId) {
            assertThat(sDevicesById.indexOfKey(deviceId)).isLessThan(0);

            InputDevice device = Shadow.newInstanceOf(InputDevice.class);
            sDevicesById.put(deviceId, device);

            ShadowInputDevice shadow = Shadow.extract(device);
            shadow.mId = deviceId;

            notifyInputDeviceAdded(deviceId);

            return shadow;
        }

        public static void detach(int deviceId) {
            detach(deviceId, /* notifyAsync= */ false);
        }

        public static void detach(int deviceId, boolean notifyAsync) {
            sDevicesById.remove(deviceId);
            (notifyAsync
                            ? (Consumer<Runnable>) ThreadUtils::postOnUiThread
                            : (Consumer<Runnable>) ThreadUtils::runOnUiThreadBlocking)
                    .accept(() -> notifyInputDeviceRemoved(deviceId));
        }

        public static void notifyInputDeviceAdded(int deviceId) {
            DeviceInput.getInstance().onInputDeviceAdded(deviceId);
        }

        public static void notifyInputDeviceRemoved(int deviceId) {
            DeviceInput.getInstance().onInputDeviceRemoved(deviceId);
        }

        public static void notifyInputDeviceChanged(int deviceId) {
            DeviceInput.getInstance().onInputDeviceChanged(deviceId);
        }

        @Resetter
        public static void reset() {
            for (int i = sDevicesById.size() - 1; i >= 0; i--) {
                detach(sDevicesById.keyAt(i));
            }
        }

        @Implementation
        public static InputDevice getDevice(int deviceId) {
            return sDevicesById.get(deviceId);
        }

        @Implementation
        public static int[] getDeviceIds() {
            int[] deviceIds = new int[sDevicesById.size()];
            for (int i = 0; i < sDevicesById.size(); i++) {
                deviceIds[i] = sDevicesById.keyAt(i);
            }
            return deviceIds;
        }

        @Implementation
        public int getId() {
            return mId;
        }

        @Implementation
        public int getKeyboardType() {
            return mKeyboardType;
        }

        public ShadowInputDevice setKeyboardType(int keyboardType) {
            if (mKeyboardType != keyboardType) {
                mKeyboardType = keyboardType;
                notifyInputDeviceChanged(mId);
            }
            return this;
        }

        @Implementation
        public int getSources() {
            return mSources;
        }

        public ShadowInputDevice addMotionRange(int axis, int source, float resolution) {
            InputDevice.MotionRange motionRange =
                    Shadow.newInstanceOf(InputDevice.MotionRange.class);
            mMotionRanges.add(motionRange);

            ShadowMotionRange shadowMotionRange = Shadow.extract(motionRange);
            shadowMotionRange.mAxis = axis;
            shadowMotionRange.mSource = source;
            shadowMotionRange.mResolution = resolution;

            notifyInputDeviceChanged(mId);
            return this;
        }

        public ShadowInputDevice setSources(int sources) {
            if (mSources != sources) {
                mSources = sources;
                notifyInputDeviceChanged(mId);
            }
            return this;
        }

        @Implementation
        public boolean isVirtual() {
            return mIsVirtual;
        }

        public ShadowInputDevice setIsVirtual(boolean isVirtual) {
            if (mIsVirtual != isVirtual) {
                mIsVirtual = isVirtual;
                notifyInputDeviceChanged(mId);
            }
            return this;
        }

        @Implementation
        public boolean supportsSource(int source) {
            return (getSources() & source) == source;
        }

        @Implementation
        public InputDevice.MotionRange getMotionRange(int axis, int source) {
            final int numRanges = mMotionRanges.size();
            for (int i = 0; i < numRanges; i++) {
                final InputDevice.MotionRange range = mMotionRanges.get(i);
                if (range.getAxis() == axis && range.getSource() == source) {
                    return range;
                }
            }
            return null;
        }

        @Implements(InputDevice.MotionRange.class)
        public static class ShadowMotionRange {
            private int mAxis;
            private int mSource;
            private float mResolution;

            public int getAxis() {
                return mAxis;
            }

            public int getSource() {
                return mSource;
            }

            public float getResolution() {
                return mResolution;
            }
        }
    }
}
