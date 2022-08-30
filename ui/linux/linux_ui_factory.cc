// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/linux_ui_factory.h"

#include <memory>
#include <utility>

#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "ui/base/buildflags.h"
#include "ui/color/system_theme.h"
#include "ui/linux/linux_ui.h"

#if BUILDFLAG(USE_GTK)
#include "ui/gtk/gtk_ui_factory.h"
#endif
#if BUILDFLAG(USE_QT)
#include "ui/qt/qt_ui.h"
#endif

namespace ui {

namespace {

std::unique_ptr<LinuxUi> CreateGtkUi() {
#if BUILDFLAG(USE_GTK)
  auto gtk_ui = BuildGtkUi();
  if (gtk_ui->Initialize())
    return gtk_ui;
#endif
  return nullptr;
}

LinuxUi* GetGtkUi() {
  static LinuxUi* gtk_ui = CreateGtkUi().release();
  return gtk_ui;
}

std::unique_ptr<LinuxUi> CreateQtUi() {
#if BUILDFLAG(USE_QT)
  auto qt_ui = qt::CreateQtUi(GetGtkUi());
  if (qt_ui->Initialize())
    return qt_ui;
#endif
  return nullptr;
}

LinuxUi* GetQtUi() {
  static LinuxUi* qt_ui = CreateQtUi().release();
  return qt_ui;
}

}  // namespace

LinuxUi* GetDefaultLinuxUi() {
  std::unique_ptr<base::Environment> env = base::Environment::Create();

  switch (GetDefaultSystemTheme()) {
    case SystemTheme::kQt:
      if (auto* qt_ui = GetQtUi())
        return qt_ui;
      return GetGtkUi();
    case SystemTheme::kGtk:
    case SystemTheme::kDefault:
      if (auto* gtk_ui = GetGtkUi())
        return gtk_ui;
      return GetQtUi();
  }
}

LinuxUi* GetLinuxUi(SystemTheme system_theme) {
  switch (system_theme) {
    case SystemTheme::kQt:
      return GetQtUi();
    case SystemTheme::kGtk:
      return GetGtkUi();
    case SystemTheme::kDefault:
      return nullptr;
  }
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
    case base::nix::DESKTOP_ENVIRONMENT_UKUI:
    case base::nix::DESKTOP_ENVIRONMENT_DEEPIN:
      return SystemTheme::kQt;
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      return SystemTheme::kDefault;
  }
}

}  // namespace ui
