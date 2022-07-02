// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/shell_dialog_linux.h"

#include "base/environment.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "ui/shell_dialogs/select_file_dialog_linux.h"
#include "ui/shell_dialogs/select_file_dialog_linux_kde.h"
#include "ui/shell_dialogs/select_file_policy.h"

#if defined(USE_DBUS)
#include "ui/shell_dialogs/select_file_dialog_linux_portal.h"
#endif

namespace ui {

namespace {

ShellDialogLinux* g_shell_dialog_linux = nullptr;

enum FileDialogChoice {
  kUnknown,
  kToolkit,
  kKde,
#if defined(USE_DBUS)
  kPortal,
#endif
};

FileDialogChoice dialog_choice_ = kUnknown;

std::string& KDialogVersion() {
  static base::NoDestructor<std::string> version;
  return *version;
}

FileDialogChoice GetFileDialogChoice() {
#if defined(USE_DBUS)
  // Check to see if the portal is available.
  if (SelectFileDialogLinuxPortal::IsPortalAvailable())
    return kPortal;
  // Make sure to kill the portal connection.
  SelectFileDialogLinuxPortal::DestroyPortalConnection();
#endif

  // Check to see if KDE is the desktop environment.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment desktop =
      base::nix::GetDesktopEnvironment(env.get());
  if (desktop == base::nix::DESKTOP_ENVIRONMENT_KDE3 ||
      desktop == base::nix::DESKTOP_ENVIRONMENT_KDE4 ||
      desktop == base::nix::DESKTOP_ENVIRONMENT_KDE5) {
    // Check to see if the user dislikes the KDE file dialog.
    if (!env->HasVar("NO_CHROME_KDE_FILE_DIALOG")) {
      // Check to see if the KDE dialog works.
      if (SelectFileDialogLinux::CheckKDEDialogWorksOnUIThread(
              KDialogVersion())) {
        return kKde;
      }
    }
  }

  return kToolkit;
}

}  // namespace

ShellDialogLinux::ShellDialogLinux() = default;

ShellDialogLinux::~ShellDialogLinux() {
#if defined(USE_DBUS)
  SelectFileDialogLinuxPortal::DestroyPortalConnection();
#endif
}

void ShellDialogLinux::SetInstance(ShellDialogLinux* instance) {
  g_shell_dialog_linux = instance;
}

const ShellDialogLinux* ShellDialogLinux::instance() {
  return g_shell_dialog_linux;
}

void ShellDialogLinux::Initialize() {
#if defined(USE_DBUS)
  SelectFileDialogLinuxPortal::StartAvailabilityTestInBackground();
#endif
}

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
  if (dialog_choice_ == kUnknown)
    dialog_choice_ = GetFileDialogChoice();

  const ShellDialogLinux* shell_dialogs = ShellDialogLinux::instance();
  switch (dialog_choice_) {
    case kToolkit:
      if (!shell_dialogs)
        break;
      return shell_dialogs->CreateSelectFileDialog(listener, std::move(policy));
#if defined(USE_DBUS)
    case kPortal:
      return new SelectFileDialogLinuxPortal(listener, std::move(policy));
#endif
    case kKde: {
      std::unique_ptr<base::Environment> env(base::Environment::Create());
      base::nix::DesktopEnvironment desktop =
          base::nix::GetDesktopEnvironment(env.get());
      return NewSelectFileDialogLinuxKde(listener, std::move(policy), desktop,
                                         KDialogVersion());
    }
    case kUnknown:
      NOTREACHED();
  }
  return nullptr;
}

}  // namespace ui
