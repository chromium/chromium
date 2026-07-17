// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device_observer_android.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/singleton.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_observer_android_test_helper.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/devices/touchscreen_device.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/devices/ui_events_devices_jni_headers/InputDeviceObserver_jni.h"

using jni_zero::AttachCurrentThread;
using jni_zero::JavaRef;
using jni_zero::ScopedJavaLocalRef;

namespace ui {

namespace {

InputDeviceType GetInputDeviceType(bool is_virtual, bool is_external) {
  if (is_virtual) {
    return INPUT_DEVICE_UNKNOWN;
  }
  // Android's InputDevice does not distinguish between USB and Bluetooth
  // external devices, so we conservatively map all external devices to USB.
  return is_external ? INPUT_DEVICE_USB : INPUT_DEVICE_INTERNAL;
}

}  // namespace

KeyboardDevice KeyboardDeviceFromJava(JNIEnv* env,
                                      const JavaRef<jobject>& j_device) {
  int id = Java_InputDeviceData_getId(env, j_device);
  std::string name = base::android::ConvertJavaStringToUTF8(
      env, Java_InputDeviceData_getName(env, j_device));
  bool is_external = Java_InputDeviceData_isExternal(env, j_device);
  bool is_virtual = Java_InputDeviceData_isVirtual(env, j_device);

  KeyboardDevice keyboard(id, GetInputDeviceType(is_virtual, is_external),
                          name);
  keyboard.vendor_id = Java_InputDeviceData_getVendorId(env, j_device);
  keyboard.product_id = Java_InputDeviceData_getProductId(env, j_device);
  return keyboard;
}

InputDevice MouseDeviceFromJava(JNIEnv* env, const JavaRef<jobject>& j_device) {
  int id = Java_InputDeviceData_getId(env, j_device);
  std::string name = base::android::ConvertJavaStringToUTF8(
      env, Java_InputDeviceData_getName(env, j_device));
  bool is_external = Java_InputDeviceData_isExternal(env, j_device);
  bool is_virtual = Java_InputDeviceData_isVirtual(env, j_device);

  InputDevice mouse(id + InputDeviceObserverAndroid::kMouseIdOffset,
                    GetInputDeviceType(is_virtual, is_external), name);
  mouse.vendor_id = Java_InputDeviceData_getVendorId(env, j_device);
  mouse.product_id = Java_InputDeviceData_getProductId(env, j_device);
  return mouse;
}

TouchpadDevice TouchpadDeviceFromJava(JNIEnv* env,
                                      const JavaRef<jobject>& j_device) {
  int id = Java_InputDeviceData_getId(env, j_device);
  std::string name = base::android::ConvertJavaStringToUTF8(
      env, Java_InputDeviceData_getName(env, j_device));
  bool is_external = Java_InputDeviceData_isExternal(env, j_device);
  bool is_virtual = Java_InputDeviceData_isVirtual(env, j_device);

  TouchpadDevice touchpad(id + InputDeviceObserverAndroid::kTouchpadIdOffset,
                          GetInputDeviceType(is_virtual, is_external), name);
  touchpad.vendor_id = Java_InputDeviceData_getVendorId(env, j_device);
  touchpad.product_id = Java_InputDeviceData_getProductId(env, j_device);
  return touchpad;
}

TouchscreenDevice TouchscreenDeviceFromJava(JNIEnv* env,
                                            const JavaRef<jobject>& j_device) {
  int id = Java_InputDeviceData_getId(env, j_device);
  std::string name = base::android::ConvertJavaStringToUTF8(
      env, Java_InputDeviceData_getName(env, j_device));
  bool is_external = Java_InputDeviceData_isExternal(env, j_device);
  bool is_virtual = Java_InputDeviceData_isVirtual(env, j_device);

  TouchscreenDevice touchscreen(
      id + InputDeviceObserverAndroid::kTouchscreenIdOffset,
      GetInputDeviceType(is_virtual, is_external), name, gfx::Size(),
      /*touch_points=*/0);
  touchscreen.vendor_id = Java_InputDeviceData_getVendorId(env, j_device);
  touchscreen.product_id = Java_InputDeviceData_getProductId(env, j_device);
  return touchscreen;
}

// Test helper implementations (declared in
// input_device_observer_android_test_helper.h)
KeyboardDevice KeyboardDeviceFromJavaForTesting(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_device) {
  return KeyboardDeviceFromJava(env, j_device);
}

InputDevice MouseDeviceFromJavaForTesting(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_device) {
  return MouseDeviceFromJava(env, j_device);
}

TouchpadDevice TouchpadDeviceFromJavaForTesting(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_device) {
  return TouchpadDeviceFromJava(env, j_device);
}

TouchscreenDevice TouchscreenDeviceFromJavaForTesting(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_device) {
  return TouchscreenDeviceFromJava(env, j_device);
}

base::android::ScopedJavaLocalRef<jobject> CreateInputDeviceDataForTesting(
    JNIEnv* env,
    int id,
    const std::string& name,
    bool is_external,
    bool is_virtual,
    int vendor_id,
    int product_id) {
  ScopedJavaLocalRef<jstring> jname =
      base::android::ConvertUTF8ToJavaString(env, name);
  return Java_InputDeviceData_Constructor(env, id, jname, is_external,
                                          is_virtual, vendor_id, product_id);
}

namespace {

void UpdateInputDevices() {
  std::vector<KeyboardDevice> keyboards;
  std::vector<InputDevice> mice;
  std::vector<TouchpadDevice> touchpads;
  std::vector<TouchscreenDevice> touchscreens;

  JNIEnv* env = AttachCurrentThread();

  // Keyboards
  ScopedJavaLocalRef<jobjectArray> j_keyboards =
      Java_InputDeviceObserver_getKeyboards(env);
  if (j_keyboards) {
    jsize size = env->GetArrayLength(j_keyboards.obj());
    for (jsize i = 0; i < size; ++i) {
      ScopedJavaLocalRef<jobject> j_device = ScopedJavaLocalRef<jobject>::Adopt(
          env, env->GetObjectArrayElement(j_keyboards.obj(), i));
      keyboards.push_back(KeyboardDeviceFromJava(env, j_device));
    }
  }

  // Mice
  ScopedJavaLocalRef<jobjectArray> j_mice =
      Java_InputDeviceObserver_getMice(env);
  if (j_mice) {
    jsize size = env->GetArrayLength(j_mice.obj());
    for (jsize i = 0; i < size; ++i) {
      ScopedJavaLocalRef<jobject> j_device = ScopedJavaLocalRef<jobject>::Adopt(
          env, env->GetObjectArrayElement(j_mice.obj(), i));
      mice.push_back(MouseDeviceFromJava(env, j_device));
    }
  }

  // Touchpads
  ScopedJavaLocalRef<jobjectArray> j_touchpads =
      Java_InputDeviceObserver_getTouchpads(env);
  if (j_touchpads) {
    jsize size = env->GetArrayLength(j_touchpads.obj());
    for (jsize i = 0; i < size; ++i) {
      ScopedJavaLocalRef<jobject> j_device = ScopedJavaLocalRef<jobject>::Adopt(
          env, env->GetObjectArrayElement(j_touchpads.obj(), i));
      touchpads.push_back(TouchpadDeviceFromJava(env, j_device));
    }
  }

  // Touchscreens
  ScopedJavaLocalRef<jobjectArray> j_touchscreens =
      Java_InputDeviceObserver_getTouchscreens(env);
  if (j_touchscreens) {
    jsize size = env->GetArrayLength(j_touchscreens.obj());
    for (jsize i = 0; i < size; ++i) {
      ScopedJavaLocalRef<jobject> j_device = ScopedJavaLocalRef<jobject>::Adopt(
          env, env->GetObjectArrayElement(j_touchscreens.obj(), i));
      touchscreens.push_back(TouchscreenDeviceFromJava(env, j_device));
    }
  }

  DeviceDataManager* device_data_manager = DeviceDataManager::GetInstance();
  DeviceHotplugEventObserver* observer = device_data_manager;

  observer->OnKeyboardDevicesUpdated(keyboards);
  observer->OnMouseDevicesUpdated(mice);
  observer->OnTouchpadDevicesUpdated(touchpads);
  observer->OnTouchscreenDevicesUpdated(touchscreens);

  if (!device_data_manager->AreDeviceListsComplete()) {
    observer->OnDeviceListsComplete();
  }
}

}  // namespace

InputDeviceObserverAndroid::InputDeviceObserverAndroid() = default;

InputDeviceObserverAndroid::~InputDeviceObserverAndroid() = default;

void InputDeviceObserverAndroid::Initialize() {
  DeviceDataManager::CreateInstance();
  UpdateInputDevices();
}

void InputDeviceObserverAndroid::Shutdown() {
  DeviceDataManager::DeleteInstance();
}

InputDeviceObserverAndroid* InputDeviceObserverAndroid::GetInstance() {
  return base::Singleton<
      InputDeviceObserverAndroid,
      base::LeakySingletonTraits<InputDeviceObserverAndroid>>::get();
}

void InputDeviceObserverAndroid::AddObserver(
    ui::InputDeviceEventObserver* observer) {
  observers_.AddObserver(observer);
  JNIEnv* env = AttachCurrentThread();
  Java_InputDeviceObserver_addObserver(env);
}

void InputDeviceObserverAndroid::RemoveObserver(
    ui::InputDeviceEventObserver* observer) {
  observers_.RemoveObserver(observer);
  JNIEnv* env = AttachCurrentThread();
  Java_InputDeviceObserver_removeObserver(env);
}

static void JNI_InputDeviceObserver_InputConfigurationChanged(JNIEnv* env) {
  InputDeviceObserverAndroid::GetInstance()
      ->UpdateAndNotifyDeviceConfigurationChanged();
}

void InputDeviceObserverAndroid::UpdateAndNotifyDeviceConfigurationChanged() {
  UpdateInputDevices();

  // TODO(crbug.com/520432048): Refactor to only notify for the specific type
  // of input device that changed instead of notifying for all types.
  observers_.Notify(
      &ui::InputDeviceEventObserver::OnInputDeviceConfigurationChanged,
      InputDeviceEventObserver::kMouse | InputDeviceEventObserver::kKeyboard |
          InputDeviceEventObserver::kTouchpad |
          InputDeviceEventObserver::kTouchscreen);
}

}  // namespace ui

DEFINE_JNI(InputDeviceObserver)
