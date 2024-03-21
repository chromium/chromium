// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/linux_ui_factory.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/memory/raw_ptr.h"
#include "base/nix/xdg_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "build/chromecast_buildflags.h"
#include "ui/base/buildflags.h"
#include "ui/base/ui_base_switches.h"
#include "ui/color/system_theme.h"
#include "ui/linux/fallback_linux_ui.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_delegate.h"

#if BUILDFLAG(USE_GTK)
#include "ui/gtk/gtk_ui_factory.h"
#endif
#if BUILDFLAG(USE_QT)
#include "ui/qt/qt_ui.h"
#endif

#if !BUILDFLAG(IS_CASTOS)
#include "ui/shell_dialogs/shell_dialog_linux.h"
#endif

namespace ui {

namespace {

std::vector<raw_ptr<LinuxUiTheme, VectorExperimental>>& GetLinuxUiThemesImpl() {
  static base::NoDestructor<
      std::vector<raw_ptr<LinuxUiTheme, VectorExperimental>>>
      themes;
  return *themes;
}

std::unique_ptr<LinuxUiAndTheme> CreateGtkUi() {
#if BUILDFLAG(USE_GTK)
  auto gtk_ui = BuildGtkUi();
  if (gtk_ui->Initialize()) {
    GetLinuxUiThemesImpl().push_back(gtk_ui.get());
    return gtk_ui;
  }
#endif
  return nullptr;
}

LinuxUiAndTheme* GetGtkUi() {
  // LinuxUi creation will fail without a delegate.
  if (!ui::LinuxUiDelegate::GetInstance()) {
    return nullptr;
  }
  static LinuxUiAndTheme* gtk_ui = CreateGtkUi().release();
  return gtk_ui;
}

std::unique_ptr<LinuxUiAndTheme> CreateQtUi() {
#if BUILDFLAG(USE_QT)
  auto qt_ui = qt::CreateQtUi(GetGtkUi());
  if (qt_ui->Initialize()) {
    GetLinuxUiThemesImpl().push_back(qt_ui.get());
    return qt_ui;
  }
#endif
  return nullptr;
}

LinuxUiAndTheme* GetQtUi() {
  // LinuxUi creation will fail without a delegate.
  if (!ui::LinuxUiDelegate::GetInstance()) {
    return nullptr;
  }
  static LinuxUiAndTheme* qt_ui = CreateQtUi().release();
  return qt_ui;
}

std::unique_ptr<LinuxUiAndTheme> CreateFallbackUi() {
  return std::make_unique<FallbackLinuxUi>();
}

LinuxUiAndTheme* GetFallbackUi() {
  static LinuxUiAndTheme* fallback_ui = CreateFallbackUi().release();
  return fallback_ui;
}

LinuxUiAndTheme* GetDefaultLinuxUiAndTheme() {
  auto* cmd_line = base::CommandLine::ForCurrentProcess();
  std::string ui_toolkit = base::ToLowerASCII(
      cmd_line->GetSwitchValueASCII(switches::kUiToolkitFlag));
  if (ui_toolkit == "gtk") {
    if (auto* gtk_ui = GetGtkUi()) {
      return gtk_ui;
    }
  } else if (ui_toolkit == "qt") {
    if (auto* qt_ui = GetQtUi()) {
      return qt_ui;
    }
  } else if (ui_toolkit == "fallback") {
    if (auto* fallback_ui = GetFallbackUi()) {
      return fallback_ui;
    }
  }

  std::unique_ptr<base::Environment> env = base::Environment::Create();
  switch (GetDefaultSystemTheme()) {
    case SystemTheme::kQt:
      if (auto* qt_ui = GetQtUi()) {
        return qt_ui;
      }
      if (auto* gtk_ui = GetGtkUi()) {
        return gtk_ui;
      }
      return GetFallbackUi();
    case SystemTheme::kGtk:
    case SystemTheme::kDefault:
      if (auto* gtk_ui = GetGtkUi()) {
        return gtk_ui;
      }
      if (auto* qt_ui = GetQtUi()) {
        return qt_ui;
      }
      return GetFallbackUi();
  }
}

}  // namespace

LinuxUi* GetDefaultLinuxUi() {
  auto* linux_ui = GetDefaultLinuxUiAndTheme();
#if !BUILDFLAG(IS_CASTOS)
  // This may create an extra thread that may race against the LinuxUi instance
  // initialization, GtkInitFromCommandLine, in GtkUi for example, so this must
  // be done after the call to GetDefaultLinuxUiAndTheme above, so the race
  // condition is avoided.
  shell_dialog_linux::Initialize();
#endif
  return linux_ui;
}

LinuxUiTheme* GetDefaultLinuxUiTheme() {
  return GetDefaultLinuxUiAndTheme();
}

LinuxUiTheme* GetLinuxUiTheme(SystemTheme system_theme) {
  switch (system_theme) {
    case SystemTheme::kQt:
      return GetQtUi();
    case SystemTheme::kGtk:
      return GetGtkUi();
    case SystemTheme::kDefault:
      return nullptr;
  }
}

const std::vector<raw_ptr<LinuxUiTheme, VectorExperimental>>&
GetLinuxUiThemes() {
  return GetLinuxUiThemesImpl();
}

SystemTheme GetDefaultSystemTheme() {
  std::unique_ptr<base::Environment> env = base::Environment::Create();

  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_CINNAMON:
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
    case base::nix::DESKTOP_ENVIRONMENT_PANTHEON:
    case base::nix::DESKTOP_ENVIRONMENT_UNITY:
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
      return SystemTheme::kGtk;
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
    case base::nix::DESKTOP_ENVIRONMENT_KDE6:
    case base::nix::DESKTOP_ENVIRONMENT_UKUI:
    case base::nix::DESKTOP_ENVIRONMENT_DEEPIN:
    case base::nix::DESKTOP_ENVIRONMENT_LXQT:
      return SystemTheme::kQt;
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      return SystemTheme::kDefault;
  }
}

}  // namespace ui
