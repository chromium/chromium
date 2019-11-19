// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/material_design/material_design_controller.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_current.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "build/buildflag.h"
#include "ui/base/buildflags.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/animation/linear_animation.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "ui/base/win/hidden_window.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"
#endif

namespace ui {

#if defined(OS_WIN)
namespace {

bool IsTabletMode() {
  return base::win::IsWindows10TabletMode(
      gfx::SingletonHwnd::GetInstance()->hwnd());
}

void TabletModeWatcherWinProc(HWND hwnd,
                              UINT message,
                              WPARAM wparam,
                              LPARAM lparam) {
  if (message == WM_SETTINGCHANGE)
    MaterialDesignController::OnTabletModeToggled(IsTabletMode());
}

}  // namespace
#endif  // defined(OS_WIN)

bool MaterialDesignController::touch_ui_ = false;
bool MaterialDesignController::automatic_touch_ui_ = false;

// static
void MaterialDesignController::Initialize() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const std::string switch_value =
      command_line->GetSwitchValueASCII(switches::kTopChromeTouchUi);
  bool touch = switch_value == switches::kTopChromeTouchUiEnabled;
  automatic_touch_ui_ = switch_value == switches::kTopChromeTouchUiAuto;

  // When the mode is not explicitly forced, platforms vary as to the default
  // behavior.
  if (!touch && (switch_value != switches::kTopChromeTouchUiDisabled) &&
      features::IsAutomaticUiAdjustmentsForTouchEnabled()) {
#if defined(OS_CHROMEOS)
    // TabletModePageBehavior's default state is in non-tablet mode.
    automatic_touch_ui_ = true;
#elif defined(OS_WIN)
    if (base::win::GetVersion() >= base::win::Version::WIN10) {
      // Win 10+ uses dynamic mode by default and checks the current tablet mode
      // state to determine whether to start in touch mode.
      automatic_touch_ui_ = true;
      if (base::MessageLoopCurrentForUI::IsSet() &&
          !GetInstance()->singleton_hwnd_observer_) {
        GetInstance()->singleton_hwnd_observer_ =
            std::make_unique<gfx::SingletonHwndObserver>(
                base::BindRepeating(TabletModeWatcherWinProc));
        touch = IsTabletMode();
      }
    }
#endif
  }
  SetTouchUi(touch);

  // TODO(crbug.com/864544): Ideally, there would be a more general, "initialize
  // random stuff here" function into which this sort of thing can be placed.
  double animation_duration_scale;
  if (base::StringToDouble(
          command_line->GetSwitchValueASCII(switches::kAnimationDurationScale),
          &animation_duration_scale)) {
    gfx::LinearAnimation::SetDurationScale(animation_duration_scale);
  }
}

// static
void MaterialDesignController::OnTabletModeToggled(bool enabled) {
  if (automatic_touch_ui_)
    SetTouchUi(enabled);
}

// static
MaterialDesignController* MaterialDesignController::GetInstance() {
  static base::NoDestructor<MaterialDesignController> instance;
  return instance.get();
}

void MaterialDesignController::AddObserver(
    MaterialDesignControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void MaterialDesignController::RemoveObserver(
    MaterialDesignControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

MaterialDesignController::MaterialDesignController() = default;

// static
void MaterialDesignController::SetTouchUi(bool touch_ui) {
  if (touch_ui_ != touch_ui) {
    touch_ui_ = touch_ui;
    for (auto& observer : GetInstance()->observers_)
      observer.OnTouchUiChanged();
  }
}

}  // namespace ui
