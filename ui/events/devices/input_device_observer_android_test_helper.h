// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_ANDROID_TEST_HELPER_H_
#define UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_ANDROID_TEST_HELPER_H_

#include <jni.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ui {

KeyboardDevice KeyboardDeviceFromJavaForTesting(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_device);

InputDevice MouseDeviceFromJavaForTesting(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_device);

TouchpadDevice TouchpadDeviceFromJavaForTesting(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_device);

TouchscreenDevice TouchscreenDeviceFromJavaForTesting(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_device);

base::android::ScopedJavaLocalRef<jobject> CreateInputDeviceDataForTesting(
    JNIEnv* env,
    int id,
    const std::string& name,
    bool is_external,
    bool is_virtual,
    int vendor_id,
    int product_id);

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_ANDROID_TEST_HELPER_H_
