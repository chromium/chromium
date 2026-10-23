// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device_observer_android.h"

#include <utility>
#include <vector>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_restrictions.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_observer_android_test_helper.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/devices/touchscreen_device.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/devices/ui_events_devices_jni_headers/InputDeviceObserver_jni.h"

using jni_zero::JavaRef;
using jni_zero::ScopedJavaLocalRef;

namespace ui {

struct InputDeviceObserverAndroid::InputDeviceLists {
  InputDeviceLists() = default;
  InputDeviceLists(InputDeviceLists&&) = default;
  InputDeviceLists& operator=(InputDeviceLists&&) = default;
  ~InputDeviceLists() = default;

  std::vector<KeyboardDevice> keyboards;
  std::vector<InputDevice> mice;
  std::vector<TouchpadDevice> touchpads;
  std::vector<TouchscreenDevice> touchscreens;
};

namespace {

InputDeviceType GetInputDeviceType(bool is_virtual, bool is_external) {
  if (is_virtual) {
    return INPUT_DEVICE_UNKNOWN;
  }
  // Android's InputDevice does not distinguish between USB and Bluetooth
  // external devices, so we conservatively map all external devices to USB.
  return is_external ? INPUT_DEVICE_USB : INPUT_DEVICE_INTERNAL;
}

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

}  // namespace

// static
InputDeviceObserverAndroid::InputDeviceLists
InputDeviceObserverAndroid::FetchInputDeviceLists() {
  base::AssertLongCPUWorkAllowed();
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  InputDeviceLists lists;
  JNIEnv* env = jni_zero::AttachCurrentThread();
  ScopedJavaLocalRef<jobjectArray> j_devices =
      Java_InputDeviceObserver_getInputDevices(env);
  if (j_devices) {
    jsize size = env->GetArrayLength(j_devices.obj());
    for (jsize i = 0; i < size; ++i) {
      ScopedJavaLocalRef<jobject> j_device = ScopedJavaLocalRef<jobject>::Adopt(
          env, env->GetObjectArrayElement(j_devices.obj(), i));
      if (Java_InputDeviceData_isKeyboard(env, j_device)) {
        lists.keyboards.push_back(KeyboardDeviceFromJava(env, j_device));
      }
      if (Java_InputDeviceData_isMouse(env, j_device)) {
        lists.mice.push_back(MouseDeviceFromJava(env, j_device));
      }
      if (Java_InputDeviceData_isTouchpad(env, j_device)) {
        lists.touchpads.push_back(TouchpadDeviceFromJava(env, j_device));
      }
      if (Java_InputDeviceData_isTouchscreen(env, j_device)) {
        lists.touchscreens.push_back(TouchscreenDeviceFromJava(env, j_device));
      }
    }
  }
  return lists;
}

InputDeviceObserverAndroid::InputDeviceObserverAndroid() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

InputDeviceObserverAndroid::~InputDeviceObserverAndroid() = default;

void InputDeviceObserverAndroid::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DeviceDataManager::CreateInstance();
  ScheduleUpdateInputDevices(/*notify_observers=*/false);
}

void InputDeviceObserverAndroid::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  update_in_flight_ = false;
  update_pending_ = false;
  notify_observers_ = false;
  DeviceDataManager::DeleteInstance();
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

InputDeviceObserverAndroid* InputDeviceObserverAndroid::GetInstance() {
  return base::Singleton<
      InputDeviceObserverAndroid,
      base::LeakySingletonTraits<InputDeviceObserverAndroid>>::get();
}

void InputDeviceObserverAndroid::AddObserver(
    ui::InputDeviceEventObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_InputDeviceObserver_addObserver(env);
}

void InputDeviceObserverAndroid::RemoveObserver(
    ui::InputDeviceEventObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_InputDeviceObserver_removeObserver(env);
}

static void JNI_InputDeviceObserver_InputConfigurationChanged(JNIEnv* env) {
  InputDeviceObserverAndroid::GetInstance()
      ->UpdateAndNotifyDeviceConfigurationChanged();
}

void InputDeviceObserverAndroid::UpdateAndNotifyDeviceConfigurationChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleUpdateInputDevices(/*notify_observers=*/true);
}

void InputDeviceObserverAndroid::ScheduleUpdateInputDevices(
    bool notify_observers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(base::ThreadPoolInstance::Get());
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  notify_observers_ |= notify_observers;
  if (update_in_flight_) {
    update_pending_ = true;
    return;
  }
  update_in_flight_ = true;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&InputDeviceObserverAndroid::FetchInputDeviceLists),
      base::BindOnce(&InputDeviceObserverAndroid::OnInputDevicesFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InputDeviceObserverAndroid::OnInputDevicesFetched(InputDeviceLists lists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  update_in_flight_ = false;
  if (!DeviceDataManager::HasInstance()) {
    update_pending_ = false;
    notify_observers_ = false;
    return;
  }
  const bool update_pending = std::exchange(update_pending_, false);
  const bool notify_observers =
      !update_pending && std::exchange(notify_observers_, false);
  ApplyInputDeviceLists(std::move(lists), notify_observers);
  if (update_pending) {
    ScheduleUpdateInputDevices(/*notify_observers=*/true);
  }
}

void InputDeviceObserverAndroid::ApplyInputDeviceLists(InputDeviceLists lists,
                                                       bool notify_observers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!DeviceDataManager::HasInstance()) {
    return;
  }
  DeviceDataManager* device_data_manager = DeviceDataManager::GetInstance();
  DeviceHotplugEventObserver* observer = device_data_manager;

  observer->OnKeyboardDevicesUpdated(lists.keyboards);
  observer->OnMouseDevicesUpdated(lists.mice);
  observer->OnTouchpadDevicesUpdated(lists.touchpads);
  observer->OnTouchscreenDevicesUpdated(lists.touchscreens);

  if (!device_data_manager->AreDeviceListsComplete()) {
    observer->OnDeviceListsComplete();
  }

  if (notify_observers) {
    // TODO(crbug.com/520432048): Refactor to only notify for the specific type
    // of input device that changed instead of notifying for all types.
    observers_.Notify(
        &ui::InputDeviceEventObserver::OnInputDeviceConfigurationChanged,
        InputDeviceEventObserver::kMouse | InputDeviceEventObserver::kKeyboard |
            InputDeviceEventObserver::kTouchpad |
            InputDeviceEventObserver::kTouchscreen);
  }
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
  return Java_InputDeviceData_Constructor(
      env, id, jname, is_external, is_virtual, vendor_id, product_id,
      /*isKeyboard=*/false, /*isMouse=*/false, /*isTouchpad=*/false,
      /*isTouchscreen=*/false);
}

}  // namespace ui

DEFINE_JNI(InputDeviceObserver)
