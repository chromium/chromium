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
#include "base/callback_list.h"
#endif

namespace ui {

// Central controller to handle touch UI modes.
class COMPONENT_EXPORT(UI_BASE) TouchUiController {
 public:
  using TouchModeCallbackList = base::RepeatingClosureList;
#if BUILDFLAG(IS_WIN)
  using TabletModeCallbackList = base::RepeatingClosureList;
#endif  // BUILDFLAG(IS_WIN)

  enum class TouchUiState {
    kDisabled,
    kAuto,
    kEnabled,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. See PostureMode in:
  // tools/metrics/histograms/metadata/input/enums.xml
  enum class PostureMode {
    kTablet = 0,
    kDesktop = 1,
    kMaxValue = kDesktop,
  };

  class COMPONENT_EXPORT(UI_BASE) TouchUiScoperForTesting {
   public:
    explicit TouchUiScoperForTesting(bool touch_ui_enabled,
                                     bool tablet_mode_enabled = false,
                                     TouchUiController* controller = Get());
    TouchUiScoperForTesting(const TouchUiScoperForTesting&) = delete;
    TouchUiScoperForTesting& operator=(const TouchUiScoperForTesting&) = delete;
    ~TouchUiScoperForTesting();

    // Update the current touch mode state but still roll back to the
    // original state at destruction. Allows a test to change the mode
    // multiple times without creating multiple instances.
    void UpdateState(bool enabled);
    void UpdateTabletMode(bool enabled);

   private:
    const raw_ptr<TouchUiController> controller_;
    const TouchUiState old_ui_state_;
    const bool old_tablet_mode_;
  };

  static TouchUiController* Get();

  explicit TouchUiController(TouchUiState touch_ui_state = TouchUiState::kAuto);
  TouchUiController(const TouchUiController&) = delete;
  TouchUiController& operator=(const TouchUiController&) = delete;
  virtual ~TouchUiController();

  // The value is indeterminate at startup, ensure that all consumers of
  // touch_ui state register a callback to get the correct initial value.
  bool touch_ui() const {
    return (touch_ui_state_ == TouchUiState::kEnabled) ||
           ((touch_ui_state_ == TouchUiState::kAuto) && tablet_mode_);
  }

#if BUILDFLAG(IS_WIN)
  bool tablet_mode() const { return tablet_mode_; }
#endif  // BUILDFLAG(IS_WIN)

  base::CallbackListSubscription RegisterCallback(
      const base::RepeatingClosure& closure);

#if BUILDFLAG(IS_WIN)
  base::CallbackListSubscription RegisterTabletModeCallback(
      const base::RepeatingClosure& closure);
#endif  // BUILDFLAG(IS_WIN)

  void OnTabletModeToggled(bool enabled);
#if BUILDFLAG(IS_WIN)
  // Check whether a device is in tablet or desktop mode in a threadpool thread,
  // and notify listeners.
  void RefreshTabletMode();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(USE_BLINK)
  void OnPointerDeviceConnected(PointerDevice::Key key);
  void OnPointerDeviceDisconnected(PointerDevice::Key key);
#endif  // BUILDFLAG(USE_BLINK)

 protected:
  TouchUiState SetTouchUiState(TouchUiState touch_ui_state);
  bool SetTabletMode(bool enable_tablet_mode);

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
#if BUILDFLAG(IS_WIN)
  void TabletModeChanged();
  // Records whether the user has entered touch mode and runs callbacks
  // if touch mode has initially been detected.
  void SetInitialTabletMode(bool enabled);
#endif  // BUILDFLAG(IS_WIN)
  TouchUiState touch_ui_state_;
  bool tablet_mode_ = false;

#if BUILDFLAG(USE_BLINK)
  void OnInitializePointerDevices();
  std::vector<PointerDevice> last_known_pointer_devices_;
#endif  // BUILDFLAG(USE_BLINK)

#if BUILDFLAG(IS_WIN)
  base::CallbackListSubscription hwnd_subscription_;
  TabletModeCallbackList tablet_mode_callback_list_;
#endif

  TouchModeCallbackList touch_mode_callback_list_;
  base::WeakPtrFactory<TouchUiController> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_BASE_POINTER_TOUCH_UI_CONTROLLER_H_
