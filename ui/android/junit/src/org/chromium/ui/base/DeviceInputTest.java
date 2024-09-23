// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static android.view.InputDevice.KEYBOARD_TYPE_ALPHABETIC;
import static android.view.InputDevice.KEYBOARD_TYPE_NONE;
import static android.view.InputDevice.KEYBOARD_TYPE_NON_ALPHABETIC;
import static android.view.InputDevice.SOURCE_KEYBOARD;
import static android.view.InputDevice.SOURCE_MOUSE;

import android.util.SparseArray;
import android.view.InputDevice;

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

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link DeviceInput}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {DeviceInputTest.ShadowInputDevice.class})
public class DeviceInputTest {

    @After
    public void tearDown() {
        ShadowInputDevice.reset();
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

    @Implements(InputDevice.class)
    public static class ShadowInputDevice extends org.robolectric.shadows.ShadowInputDevice {

        private static final SparseArray<InputDevice> sDevicesById = new SparseArray<>();

        private int mId;
        private boolean mIsVirtual;
        private int mKeyboardType;
        private int mSources;

        public static ShadowInputDevice attach(int deviceId) {
            assert sDevicesById.indexOfKey(deviceId) < 0;

            InputDevice device = Shadow.newInstanceOf(InputDevice.class);
            sDevicesById.put(deviceId, device);

            ShadowInputDevice shadow = Shadow.extract(device);
            shadow.mId = deviceId;

            DeviceInput.getInstance().onInputDeviceAdded(deviceId);

            return shadow;
        }

        public static void detach(int deviceId) {
            sDevicesById.remove(deviceId);
            DeviceInput.getInstance().onInputDeviceRemoved(deviceId);
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
                DeviceInput.getInstance().onInputDeviceChanged(mId);
            }
            return this;
        }

        @Implementation
        public int getSources() {
            return mSources;
        }

        public ShadowInputDevice setSources(int sources) {
            if (mSources != sources) {
                mSources = sources;
                DeviceInput.getInstance().onInputDeviceChanged(mId);
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
                DeviceInput.getInstance().onInputDeviceChanged(mId);
            }
            return this;
        }

        @Implementation
        public boolean supportsSource(int source) {
            return (getSources() & source) == source;
        }
    }
}
