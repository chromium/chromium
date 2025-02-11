// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_POINTER_TOUCH_UI_CONTROLLER_H_
#define UI_BASE_POINTER_TOUCH_UI_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(USE_BLINK)
#include "ui/base/pointer/pointer_device.h"
#endif  // BUILDFLAG(USE_BLINK)

#if BUILDFLAG(IS_WIN)
namespace gfx {
class SingletonHwndObserver;
}
#endif

namespace ui {

// Central controller to handle touch UI modes.
class COMPONENT_EXPORT(UI_BASE) TouchUiController {
 public:
  using CallbackList = base::RepeatingClosureList;

  enum class TouchUiState {
    kDisabled,
    kAuto,
    kEnabled,
  };

  class COMPONENT_EXPORT(UI_BASE) TouchUiScoperForTesting {
   public:
    explicit TouchUiScoperForTesting(bool enabled,
                                     TouchUiController* controller = Get());
    TouchUiScoperForTesting(const TouchUiScoperForTesting&) = delete;
    TouchUiScoperForTesting& operator=(const TouchUiScoperForTesting&) = delete;
    ~TouchUiScoperForTesting();

    // Update the current touch mode state but still roll back to the
    // original state at destruction. Allows a test to change the mode
    // multiple times without creating multiple instances.
    void UpdateState(bool enabled);

   private:
    const raw_ptr<TouchUiController> controller_;
    const TouchUiState old_state_;
  };

  static TouchUiController* Get();

  explicit TouchUiController(TouchUiState touch_ui_state = TouchUiState::kAuto);
  TouchUiController(const TouchUiController&) = delete;
  TouchUiController& operator=(const TouchUiController&) = delete;
  virtual ~TouchUiController();

  bool touch_ui() const {
    return (touch_ui_state_ == TouchUiState::kEnabled) ||
           ((touch_ui_state_ == TouchUiState::kAuto) && tablet_mode_);
  }

  base::CallbackListSubscription RegisterCallback(
      const base::RepeatingClosure& closure);

  void OnTabletModeToggled(bool enabled);

#if BUILDFLAG(USE_BLINK)
  void OnPointerDeviceConnected(PointerDevice::Key key);
  void OnPointerDeviceDisconnected(PointerDevice::Key key);
#endif  // BUILDFLAG(USE_BLINK)

 protected:
  TouchUiState SetTouchUiState(TouchUiState touch_ui_state);

#if BUILDFLAG(USE_BLINK)
  virtual int MaxTouchPoints() const;
  virtual std::optional<PointerDevice> GetPointerDevice(
      PointerDevice::Key key) const;
  virtual std::vector<PointerDevice> GetPointerDevices() const;
  const std::vector<PointerDevice>& GetLastKnownPointerDevicesForTesting()
      const;
#endif  // BUILDFLAG(USE_BLINK)

 private:
  void TouchUiChanged();

  bool tablet_mode_ = false;
  TouchUiState touch_ui_state_;

#if BUILDFLAG(USE_BLINK)
  void OnInitializePointerDevices();
  std::vector<PointerDevice> last_known_pointer_devices_;
#endif  // BUILDFLAG(USE_BLINK)

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;
#endif

  CallbackList callback_list_;
  base::WeakPtrFactory<TouchUiController> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_POINTER_TOUCH_UI_CONTROLLER_H_
