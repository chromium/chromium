// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device_observer_android.h"

#include <jni.h>

#include <optional>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/singleton.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_observer_android_test_helper.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/devices/touchscreen_device.h"

using base::android::ScopedJavaLocalRef;

namespace ui {

namespace {

class FakeInputDeviceEventObserver : public ui::InputDeviceEventObserver {
 public:
  FakeInputDeviceEventObserver() = default;
  ~FakeInputDeviceEventObserver() override = default;

  void OnInputDeviceConfigurationChanged(uint8_t types) override {
    notified_types_ = types;
  }

  const std::optional<uint8_t>& notified_types() const {
    return notified_types_;
  }
  void reset() { notified_types_.reset(); }

 private:
  std::optional<uint8_t> notified_types_;
};

}  // namespace

class InputDeviceObserverAndroidTest : public testing::Test {
 public:
  InputDeviceObserverAndroidTest() = default;
  ~InputDeviceObserverAndroidTest() override = default;

  void TearDown() override { DeviceDataManager::DeleteInstance(); }
};

TEST_F(InputDeviceObserverAndroidTest, InitializesDeviceDataManager) {
  InputDeviceObserverAndroid* observer =
      InputDeviceObserverAndroid::GetInstance();
  ASSERT_TRUE(observer);

  EXPECT_FALSE(DeviceDataManager::HasInstance());

  observer->Initialize();

  EXPECT_TRUE(DeviceDataManager::HasInstance());
  DeviceDataManager* manager = DeviceDataManager::GetInstance();
  ASSERT_TRUE(manager);

  EXPECT_TRUE(manager->AreDeviceListsComplete());
}

TEST_F(InputDeviceObserverAndroidTest, AddObserverNotification) {
  InputDeviceObserverAndroid* observer =
      InputDeviceObserverAndroid::GetInstance();
  ASSERT_TRUE(observer);
  observer->Initialize();

  FakeInputDeviceEventObserver mock_observer;
  observer->AddObserver(&mock_observer);

  EXPECT_FALSE(mock_observer.notified_types().has_value());

  observer->UpdateAndNotifyDeviceConfigurationChanged();

  EXPECT_TRUE(mock_observer.notified_types().has_value());
  EXPECT_EQ(*mock_observer.notified_types(),
            InputDeviceEventObserver::kMouse |
                InputDeviceEventObserver::kKeyboard |
                InputDeviceEventObserver::kTouchpad |
                InputDeviceEventObserver::kTouchscreen);

  observer->RemoveObserver(&mock_observer);
}

TEST_F(InputDeviceObserverAndroidTest, AddKeyboardDevice) {
  JNIEnv* env = jni_zero::AttachCurrentThread();

  // Test external physical keyboard (USB/Bluetooth) -> INPUT_DEVICE_USB
  ScopedJavaLocalRef<jobject> j_device = CreateInputDeviceDataForTesting(
      env, /*id=*/1, "Test Keyboard",
      /*is_external=*/true, /*is_virtual=*/false,
      /*vendor_id=*/0x1111, /*product_id=*/0x2222);
  KeyboardDevice keyboard = KeyboardDeviceFromJavaForTesting(env, j_device);

  EXPECT_EQ(keyboard.id, 1);
  EXPECT_EQ(keyboard.name, "Test Keyboard");
  EXPECT_EQ(keyboard.type, INPUT_DEVICE_USB);
  EXPECT_EQ(keyboard.vendor_id, 0x1111);
  EXPECT_EQ(keyboard.product_id, 0x2222);

  // Test internal physical keyboard -> INPUT_DEVICE_INTERNAL
  j_device = CreateInputDeviceDataForTesting(
      env, /*id=*/2, "Test Keyboard",
      /*is_external=*/false, /*is_virtual=*/false,
      /*vendor_id=*/0x3333, /*product_id=*/0x4444);
  keyboard = KeyboardDeviceFromJavaForTesting(env, j_device);

  EXPECT_EQ(keyboard.type, INPUT_DEVICE_INTERNAL);

  // Test virtual keyboard -> INPUT_DEVICE_UNKNOWN
  j_device = CreateInputDeviceDataForTesting(
      env, /*id=*/3, "Test Keyboard",
      /*is_external=*/true, /*is_virtual=*/true,
      /*vendor_id=*/0x5555, /*product_id=*/0x6666);
  keyboard = KeyboardDeviceFromJavaForTesting(env, j_device);

  EXPECT_EQ(keyboard.type, INPUT_DEVICE_UNKNOWN);
}

TEST_F(InputDeviceObserverAndroidTest, AddMouseDevice) {
  JNIEnv* env = jni_zero::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_device = CreateInputDeviceDataForTesting(
      env, /*id=*/1, "Test Mouse",
      /*is_external=*/true, /*is_virtual=*/false,
      /*vendor_id=*/0x1111, /*product_id=*/0x2222);
  InputDevice mouse = MouseDeviceFromJavaForTesting(env, j_device);

  EXPECT_EQ(mouse.id, 1 + InputDeviceObserverAndroid::kMouseIdOffset);
  EXPECT_EQ(mouse.type, INPUT_DEVICE_USB);
}

TEST_F(InputDeviceObserverAndroidTest, AddTouchpadDevice) {
  JNIEnv* env = jni_zero::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_device = CreateInputDeviceDataForTesting(
      env, /*id=*/1, "Test Touchpad",
      /*is_external=*/false, /*is_virtual=*/false,
      /*vendor_id=*/0x1111, /*product_id=*/0x2222);
  TouchpadDevice touchpad = TouchpadDeviceFromJavaForTesting(env, j_device);

  EXPECT_EQ(touchpad.id, 1 + InputDeviceObserverAndroid::kTouchpadIdOffset);
  EXPECT_EQ(touchpad.type, INPUT_DEVICE_INTERNAL);
}

TEST_F(InputDeviceObserverAndroidTest, AddTouchscreenDevice) {
  JNIEnv* env = jni_zero::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> j_device = CreateInputDeviceDataForTesting(
      env, /*id=*/1, "Test Touchscreen",
      /*is_external=*/false, /*is_virtual=*/false,
      /*vendor_id=*/0x1111, /*product_id=*/0x2222);
  TouchscreenDevice touchscreen =
      TouchscreenDeviceFromJavaForTesting(env, j_device);

  EXPECT_EQ(touchscreen.id,
            1 + InputDeviceObserverAndroid::kTouchscreenIdOffset);
  EXPECT_EQ(touchscreen.type, INPUT_DEVICE_INTERNAL);
}

}  // namespace ui
