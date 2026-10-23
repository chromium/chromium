// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_ANDROID_H_
#define UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_ANDROID_H_

#include <jni.h>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ui {

// This class is a singleton responsible to notify the
// InputDeviceChangeObserver whenever an input change
// happened on the Java side.
class EVENTS_DEVICES_EXPORT InputDeviceObserverAndroid {
 public:
  // Offsets to generate unique C++ device IDs (pseudo-IDs) for devices with
  // multiple capabilities. Java device IDs are only unique per physical device.
  // We assume Java IDs are always < 1,000,000, as it is practically impossible
  // to have 1M devices connected. This prevents clashes (e.g., physical device
  // 1 with keyboard and mouse maps to C++ IDs 1 and 1000001). See:
  // https://cs.android.com/android/platform/superproject/+/main:frameworks/native/services/inputflinger/reader/EventHub.cpp
  static constexpr int32_t kMouseIdOffset = 1000000;
  static constexpr int32_t kTouchpadIdOffset = 2000000;
  static constexpr int32_t kTouchscreenIdOffset = 3000000;

  InputDeviceObserverAndroid(const InputDeviceObserverAndroid&) = delete;
  InputDeviceObserverAndroid& operator=(const InputDeviceObserverAndroid&) =
      delete;

  ~InputDeviceObserverAndroid();

  // Returns the singleton instance of InputDeviceObserverAndroid.
  static InputDeviceObserverAndroid* GetInstance();

  // Initializes the DeviceDataManager instance and asynchronously fetches
  // initial input devices from Java on the thread pool (so
  // DeviceDataManager::AreDeviceListsComplete() is populated asynchronously
  // rather than synchronously on return). This should be called on the UI
  // thread before using DeviceDataManager or displaying UI.
  void Initialize();

  // Destroys the DeviceDataManager instance and cleans up resources.
  void Shutdown();

  // Adds an observer to receive notifications when input devices change.
  // If this is the first observer, it will also register the Java-side
  // listener.
  void AddObserver(ui::InputDeviceEventObserver* observer);

  // Removes a registered observer. If no observers remain, it will unregister
  // the Java-side listener.
  void RemoveObserver(ui::InputDeviceEventObserver* observer);

  // Asynchronously refreshes the active input devices list from the Java side
  // on the thread pool and notifies all registered observers of the changes on
  // the UI thread. Called from Java JNI when the system reports an input device
  // configuration change.
  void UpdateAndNotifyDeviceConfigurationChanged();

 private:
  struct InputDeviceLists;

  InputDeviceObserverAndroid();

  static InputDeviceLists FetchInputDeviceLists();

  void ScheduleUpdateInputDevices(bool notify_observers);
  void OnInputDevicesFetched(InputDeviceLists lists);
  void ApplyInputDeviceLists(InputDeviceLists lists, bool notify_observers);

  SEQUENCE_CHECKER(sequence_checker_);

  bool update_in_flight_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool update_pending_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool notify_observers_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  base::ObserverList<ui::InputDeviceEventObserver>::Unchecked observers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<InputDeviceObserverAndroid> weak_ptr_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

  friend struct base::DefaultSingletonTraits<InputDeviceObserverAndroid>;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_INPUT_DEVICE_OBSERVER_ANDROID_H_
