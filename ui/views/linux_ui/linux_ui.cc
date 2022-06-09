// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/linux_ui/linux_ui.h"

#include <cstdio>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "build/build_config.h"
#include "ui/base/ime/linux/linux_input_method_context_factory.h"
#include "ui/gfx/skia_font_delegate.h"

namespace {

views::LinuxUI* g_linux_ui = nullptr;

}  // namespace

namespace views {

void LinuxUI::SetInstance(std::unique_ptr<LinuxUI> instance) {
  delete g_linux_ui;
  g_linux_ui = instance.release();

  SkiaFontDelegate::SetInstance(g_linux_ui);
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)
  ShellDialogLinux::SetInstance(g_linux_ui);
#endif
  ui::SetTextEditKeyBindingsDelegate(g_linux_ui);

  // Do not set IME instance for ozone as we delegate creating the input method
  // to OzonePlatforms instead. If this is set, OzonePlatform never sets a
  // context factory.
}

LinuxUI* LinuxUI::instance() {
  return g_linux_ui;
}

LinuxUI::LinuxUI() = default;

LinuxUI::~LinuxUI() = default;

LinuxUI::CmdLineArgs::CmdLineArgs() = default;

LinuxUI::CmdLineArgs::CmdLineArgs(const CmdLineArgs&) = default;

LinuxUI::CmdLineArgs& LinuxUI::CmdLineArgs::operator=(const CmdLineArgs&) =
    default;

LinuxUI::CmdLineArgs::~CmdLineArgs() = default;

void LinuxUI::AddWindowButtonOrderObserver(
    views::WindowButtonOrderObserver* observer) {
  window_button_order_observer_list_.AddObserver(observer);
}

void LinuxUI::RemoveWindowButtonOrderObserver(
    views::WindowButtonOrderObserver* observer) {
  window_button_order_observer_list_.RemoveObserver(observer);
}

void LinuxUI::AddDeviceScaleFactorObserver(
    views::DeviceScaleFactorObserver* observer) {
  device_scale_factor_observer_list_.AddObserver(observer);
}

void LinuxUI::RemoveDeviceScaleFactorObserver(
    views::DeviceScaleFactorObserver* observer) {
  device_scale_factor_observer_list_.RemoveObserver(observer);
}

ui::NativeTheme* LinuxUI::GetNativeTheme(aura::Window* window) const {
  return GetNativeTheme(use_system_theme_callback_.is_null() ||
                        use_system_theme_callback_.Run(window));
}

ui::NativeTheme* LinuxUI::GetNativeTheme(bool use_system_theme) const {
  return use_system_theme ? GetNativeTheme()
                          : ui::NativeTheme::GetInstanceForNativeUi();
}

void LinuxUI::SetUseSystemThemeCallback(UseSystemThemeCallback callback) {
  use_system_theme_callback_ = std::move(callback);
}

bool LinuxUI::GetDefaultUsesSystemTheme() const {
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
LinuxUI::CmdLineArgs LinuxUI::CopyCmdLine(
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

}  // namespace views
