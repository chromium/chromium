// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/linux_ui.h"

#include <cstdio>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "ui/linux/cursor_theme_manager_observer.h"

namespace {

std::unique_ptr<ui::LinuxUi>& GetLinuxUiInstance() {
  static base::NoDestructor<std::unique_ptr<ui::LinuxUi>> linux_ui;
  return *linux_ui;
}

}  // namespace

namespace ui {

// static
std::unique_ptr<LinuxUi> LinuxUi::SetInstance(
    std::unique_ptr<LinuxUi> instance) {
  return std::exchange(GetLinuxUiInstance(), std::move(instance));
}

// static
LinuxUi* LinuxUi::instance() {
  return GetLinuxUiInstance().get();
}

LinuxUi::LinuxUi() = default;

LinuxUi::~LinuxUi() = default;

LinuxUi::CmdLineArgs::CmdLineArgs() = default;

LinuxUi::CmdLineArgs::CmdLineArgs(CmdLineArgs&&) = default;

LinuxUi::CmdLineArgs& LinuxUi::CmdLineArgs::operator=(CmdLineArgs&&) = default;

LinuxUi::CmdLineArgs::~CmdLineArgs() = default;

void LinuxUi::AddWindowButtonOrderObserver(
    WindowButtonOrderObserver* observer) {
  window_button_order_observer_list_.AddObserver(observer);
}

void LinuxUi::RemoveWindowButtonOrderObserver(
    WindowButtonOrderObserver* observer) {
  window_button_order_observer_list_.RemoveObserver(observer);
}

void LinuxUi::AddDeviceScaleFactorObserver(
    DeviceScaleFactorObserver* observer) {
  device_scale_factor_observer_list_.AddObserver(observer);
}

void LinuxUi::RemoveDeviceScaleFactorObserver(
    DeviceScaleFactorObserver* observer) {
  device_scale_factor_observer_list_.RemoveObserver(observer);
}

void LinuxUi::AddCursorThemeObserver(CursorThemeManagerObserver* observer) {
  cursor_theme_observer_list_.AddObserver(observer);
  std::string name = GetCursorThemeName();
  if (!name.empty())
    observer->OnCursorThemeNameChanged(name);
  int size = GetCursorThemeSize();
  if (size)
    observer->OnCursorThemeSizeChanged(size);
}

void LinuxUi::RemoveCursorThemeObserver(CursorThemeManagerObserver* observer) {
  cursor_theme_observer_list_.RemoveObserver(observer);
}

ui::NativeTheme* LinuxUi::GetNativeTheme(aura::Window* window) const {
  return GetNativeTheme(use_system_theme_callback_.is_null() ||
                        use_system_theme_callback_.Run(window));
}

void LinuxUi::SetUseSystemThemeCallback(UseSystemThemeCallback callback) {
  use_system_theme_callback_ = std::move(callback);
}

bool LinuxUi::GetDefaultUsesSystemTheme() const {
  std::unique_ptr<base::Environment> env = base::Environment::Create();

  // TODO(https://crbug.com/1317782): This logic won't be necessary after
  // the GTK/QT backend is chosen based on the environment.
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_CINNAMON:
    case base::nix::DESKTOP_ENVIRONMENT_DEEPIN:
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
    case base::nix::DESKTOP_ENVIRONMENT_PANTHEON:
    case base::nix::DESKTOP_ENVIRONMENT_UKUI:
    case base::nix::DESKTOP_ENVIRONMENT_UNITY:
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
      return true;
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      return false;
  }
}

// static
LinuxUi::CmdLineArgs LinuxUi::CopyCmdLine(
    const base::CommandLine& command_line) {
  const auto& argv = command_line.argv();
  size_t args_chars = 0;
  for (const auto& arg : argv)
    args_chars += arg.size() + 1;

  CmdLineArgs cmd_line;
  cmd_line.args = std::vector<char>(args_chars);
  char* dst = cmd_line.args.data();
  for (const auto& arg : argv) {
    cmd_line.argv.push_back(dst);
    snprintf(dst, &cmd_line.args.back() + 1 - dst, "%s", arg.c_str());
    dst += arg.size() + 1;
  }
  cmd_line.argc = cmd_line.argv.size();

  return cmd_line;
}

}  // namespace ui
