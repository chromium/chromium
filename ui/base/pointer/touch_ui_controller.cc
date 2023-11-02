// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/pointer/touch_ui_controller.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"
#endif

namespace ui {

namespace {

#if BUILDFLAG(IS_WIN)

bool IsTabletMode() {
  return base::win::IsWindows10OrGreaterTabletMode(
      gfx::SingletonHwnd::GetInstance()->hwnd());
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
#if BUILDFLAG(IS_WIN)
  if (base::CurrentUIThread::IsSet() &&
      base::win::GetVersion() >= base::win::Version::WIN10) {
    singleton_hwnd_observer_ =
        std::make_unique<gfx::SingletonHwndObserver>(base::BindRepeating(
            [](HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
              if (message == WM_SETTINGCHANGE)
                Get()->OnTabletModeToggled(IsTabletMode());
            }));
    tablet_mode_ = IsTabletMode();
  }
#endif

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

}  // namespace ui
