// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/touch_ui_controller.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"
#endif

namespace ui {

namespace {

#if BUILDFLAG(USE_BLINK)
void RecordPointerDigitizerTypeOnStartup(const PointerDevice& device) {
  base::UmaHistogramEnumeration("Input.Digitizer.OnStartup", device.digitizer);
}

void RecordPointerDigitizerTypeOnConnected(const PointerDevice& device) {
  base::UmaHistogramEnumeration("Input.Digitizer.OnConnected",
                                device.digitizer);
}

void RecordPointerDigitizerTypeOnDisconnected(const PointerDevice& device) {
  base::UmaHistogramEnumeration("Input.Digitizer.OnDisconnected",
                                device.digitizer);
}

void RecordMaxTouchPointsHistogram(const char* histogram_name, int32_t sample) {
  // Record max touch points. Maximum value is 30, after which the values will
  // be recorded in an overflow bucket. The maximum of 30 is chosen based on 3
  // people interacting with both hands on a screen simultaneously.
  base::UmaHistogramExactLinear(histogram_name, sample, 30);
}

constexpr const char* GetMaxTouchPointsHistogramName(
    PointerDigitizerType type) {
  switch (type) {
    case PointerDigitizerType::kUnknown:
      return "Input.Digitizer.MaxTouchPoints.Unknown";
    case PointerDigitizerType::kDirectPen:
      return "Input.Digitizer.MaxTouchPoints.DirectPen";
    case PointerDigitizerType::kIndirectPen:
      return "Input.Digitizer.MaxTouchPoints.IndirectPen";
    case PointerDigitizerType::kTouch:
      return "Input.Digitizer.MaxTouchPoints.Touch";
    case PointerDigitizerType::kTouchPad:
      return "Input.Digitizer.MaxTouchPoints.TouchPad";
  }
  NOTREACHED();
}

void RecordPointerDigitizerTypeMaxTouchPoints(const PointerDevice& device) {
  RecordMaxTouchPointsHistogram(
      GetMaxTouchPointsHistogramName(device.digitizer),
      device.max_active_contacts);
}

void RecordMaxTouchPointsSupportedBySystem(int max_touch_points) {
  RecordMaxTouchPointsHistogram(
      "Input.Digitizer.MaxTouchPointsSupportedBySystemAtStartup",
      max_touch_points);
}
#endif  // BUILDFLAG(USE_BLINK)

#if BUILDFLAG(IS_WIN)

bool IsTabletMode() {
  return base::win::IsWindows10OrGreaterTabletMode(
      gfx::SingletonHwnd::GetInstance()->hwnd());
}

bool IsWndProcMessageObserved(UINT message) {
#if BUILDFLAG(USE_BLINK)
  return message == WM_SETTINGCHANGE || message == WM_POINTERDEVICECHANGE;
#elif   // BUILDFLAG(USE_BLINK)
  return message == WM_SETTINGCHANGE;
#endif  // BUILDFLAG(USE_BLINK)
}

void SequencedWndProcHandler(UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_SETTINGCHANGE:
      TouchUiController::Get()->OnTabletModeToggled(IsTabletMode());
      break;
#if BUILDFLAG(USE_BLINK)
    case WM_POINTERDEVICECHANGE:
      if (wparam == PDC_ARRIVAL) {
        TouchUiController::Get()->OnPointerDeviceConnected(
            reinterpret_cast<HANDLE>(lparam));
      } else if (wparam == PDC_REMOVAL) {
        TouchUiController::Get()->OnPointerDeviceDisconnected(
            reinterpret_cast<HANDLE>(lparam));
      }
      break;
#endif  // BUILDFLAG(USE_BLINK)
    default:
      NOTREACHED();
  }
}

void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  if (!IsWndProcMessageObserved(message)) {
    return;
  }
  // Pass the work to a separate task to avoid possible jank when handling
  // winapi events. This also makes sure events are processed after
  // OnInitializePointerDevices which needs to run before handling
  // WM_POINTERDEVICECHANGE.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SequencedWndProcHandler, message, wparam, lparam));
}

#endif  // BUILDFLAG(IS_WIN)

void RecordEnteredTouchMode() {
  base::RecordAction(base::UserMetricsAction("TouchMode.EnteredTouchMode"));
}

void RecordEnteredNonTouchMode() {
  base::RecordAction(base::UserMetricsAction("TouchMode.EnteredNonTouchMode"));
}

}  // namespace

TouchUiController::TouchUiScoperForTesting::TouchUiScoperForTesting(
    bool enabled,
    TouchUiController* controller)
    : controller_(controller),
      old_state_(controller_->SetTouchUiState(
          enabled ? TouchUiState::kEnabled : TouchUiState::kDisabled)) {}

TouchUiController::TouchUiScoperForTesting::~TouchUiScoperForTesting() {
  controller_->SetTouchUiState(old_state_);
}

void TouchUiController::TouchUiScoperForTesting::UpdateState(bool enabled) {
  controller_->SetTouchUiState(enabled ? TouchUiState::kEnabled
                                       : TouchUiState::kDisabled);
}

// static
TouchUiController* TouchUiController::Get() {
  static base::NoDestructor<TouchUiController> instance([] {
    const std::string switch_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kTopChromeTouchUi);
    if (switch_value == switches::kTopChromeTouchUiDisabled)
      return TouchUiState::kDisabled;
    const bool enabled = switch_value == switches::kTopChromeTouchUiEnabled;
    return enabled ? TouchUiState::kEnabled : TouchUiState::kAuto;
  }());
  return instance.get();
}

TouchUiController::TouchUiController(TouchUiState touch_ui_state)
    : touch_ui_state_(touch_ui_state) {
  if (base::CurrentUIThread::IsSet()) {
#if BUILDFLAG(USE_BLINK)
    // Pass the work to a separate task to avoid affecting browser startup time.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TouchUiController::OnInitializePointerDevices,
                       weak_factory_.GetWeakPtr()));
#if BUILDFLAG(IS_WIN)
    // Register to listen for WM_POINTERDEVICECHANGE.
    base::win::RegisterPointerDeviceNotifications(
        gfx::SingletonHwnd::GetInstance()->hwnd(),
        /*notify_proximity_changes=*/false);
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(USE_BLINK)

#if BUILDFLAG(IS_WIN)
    singleton_hwnd_observer_ = std::make_unique<gfx::SingletonHwndObserver>(
        base::BindRepeating(&OnWndProc));
    tablet_mode_ = IsTabletMode();
#endif
  }

  if (touch_ui())
    RecordEnteredTouchMode();
  else
    RecordEnteredNonTouchMode();
}

TouchUiController::~TouchUiController() = default;

void TouchUiController::OnTabletModeToggled(bool enabled) {
  const bool was_touch_ui = touch_ui();
  tablet_mode_ = enabled;
  if (touch_ui() != was_touch_ui)
    TouchUiChanged();
}

base::CallbackListSubscription TouchUiController::RegisterCallback(
    const base::RepeatingClosure& closure) {
  return callback_list_.Add(closure);
}

TouchUiController::TouchUiState TouchUiController::SetTouchUiState(
    TouchUiState touch_ui_state) {
  const bool was_touch_ui = touch_ui();
  const TouchUiState old_state = std::exchange(touch_ui_state_, touch_ui_state);
  if (touch_ui() != was_touch_ui)
    TouchUiChanged();
  return old_state;
}

void TouchUiController::TouchUiChanged() {
  if (touch_ui())
    RecordEnteredTouchMode();
  else
    RecordEnteredNonTouchMode();

  TRACE_EVENT0("ui", "TouchUiController.NotifyListeners");
  callback_list_.Notify();
}

#if BUILDFLAG(USE_BLINK)
void TouchUiController::OnPointerDeviceConnected(PointerDevice::Key key) {
  if (const std::optional<PointerDevice> device = GetPointerDevice(key)) {
    RecordPointerDigitizerTypeOnConnected(*device);
    RecordPointerDigitizerTypeMaxTouchPoints(*device);
    last_known_pointer_devices_.emplace_back(*device);
  }
}

void TouchUiController::OnPointerDeviceDisconnected(PointerDevice::Key key) {
  // Iterative search should be fine because `last_known_pointer_devices_` is
  // expected to be a very small set.
  const auto iter = std::find(last_known_pointer_devices_.begin(),
                              last_known_pointer_devices_.end(), key);
  if (iter != last_known_pointer_devices_.end()) {
    RecordPointerDigitizerTypeOnDisconnected(*iter);
    last_known_pointer_devices_.erase(iter);
  }
}

int TouchUiController::MaxTouchPoints() const {
  return ::ui::MaxTouchPoints();
}

std::optional<PointerDevice> TouchUiController::GetPointerDevice(
    PointerDevice::Key key) const {
  return ::ui::GetPointerDevice(key);
}

std::vector<PointerDevice> TouchUiController::GetPointerDevices() const {
  return ::ui::GetPointerDevices();
}

const std::vector<PointerDevice>&
TouchUiController::GetLastKnownPointerDevicesForTesting() const {
  return last_known_pointer_devices_;
}

void TouchUiController::OnInitializePointerDevices() {
  last_known_pointer_devices_ = GetPointerDevices();
  for (const PointerDevice& device : last_known_pointer_devices_) {
    RecordPointerDigitizerTypeOnStartup(device);
    RecordPointerDigitizerTypeMaxTouchPoints(device);
  }
  RecordMaxTouchPointsSupportedBySystem(MaxTouchPoints());
}
#endif  // BUILDFLAG(USE_BLINK)

}  // namespace ui
