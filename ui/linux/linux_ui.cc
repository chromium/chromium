// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/linux/linux_ui.h"

#include <cstdio>
#include <utility>

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "ui/linux/cursor_theme_manager_observer.h"
#include "ui/linux/linux_ui_getter.h"

namespace ui {

namespace {

LinuxUi* g_linux_ui = nullptr;

}  // namespace

// static
LinuxUi* LinuxUi::SetInstance(LinuxUi* instance) {
  return std::exchange(g_linux_ui, instance);
}

// static
LinuxUi* LinuxUi::instance() {
  return g_linux_ui;
}

LinuxUi::LinuxUi() = default;

LinuxUi::~LinuxUi() = default;

LinuxUi::CmdLineArgs::CmdLineArgs() = default;

LinuxUi::CmdLineArgs::CmdLineArgs(CmdLineArgs&&) = default;

LinuxUi::CmdLineArgs& LinuxUi::CmdLineArgs::operator=(CmdLineArgs&&) = default;

LinuxUi::CmdLineArgs::~CmdLineArgs() = default;

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
  if (!name.empty()) {
    observer->OnCursorThemeNameChanged(name);
  }
  int size = GetCursorThemeSize();
  if (size) {
    observer->OnCursorThemeSizeChanged(size);
  }
}

void LinuxUi::RemoveCursorThemeObserver(CursorThemeManagerObserver* observer) {
  cursor_theme_observer_list_.RemoveObserver(observer);
}

LinuxUi::FontSettings LinuxUi::GetDefaultFontDescription() {
  if (!default_font_settings_.has_value()) {
    InitializeFontSettings();
  }
  return *default_font_settings_;
}

// static
LinuxUi::CmdLineArgs LinuxUi::CopyCmdLine(
    const base::CommandLine& command_line) {
  const auto& argv = command_line.argv();
  size_t args_chars = 0;
  for (const auto& arg : argv) {
    args_chars += arg.size() + 1;
  }

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

LinuxUiTheme::LinuxUiTheme() = default;

LinuxUiTheme::~LinuxUiTheme() = default;

// static
LinuxUiTheme* LinuxUiTheme::GetForWindow(aura::Window* window) {
  if (auto* getter = LinuxUiGetter::instance()) {
    return getter->GetForWindow(window);
  }
  return nullptr;
}

// static
LinuxUiTheme* LinuxUiTheme::GetForProfile(Profile* profile) {
  if (auto* getter = LinuxUiGetter::instance()) {
    return getter->GetForProfile(profile);
  }
  return nullptr;
}

}  // namespace ui
