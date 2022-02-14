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
#include "ui/shell_dialogs/select_file_dialog_linux_portal.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace {

ui::ShellDialogLinux* g_shell_dialog_linux = nullptr;

enum FileDialogChoice { kUnknown, kToolkit, kKde, kPortal };

FileDialogChoice dialog_choice_ = kUnknown;

std::string& KDialogVersion() {
  static base::NoDestructor<std::string> version;
  return *version;
}

}  // namespace

namespace ui {

ShellDialogLinux::ShellDialogLinux() = default;

ShellDialogLinux::~ShellDialogLinux() {
  SelectFileDialogLinuxPortal::DestroyPortalConnection();
}

void ShellDialogLinux::SetInstance(ShellDialogLinux* instance) {
  g_shell_dialog_linux = instance;
}

const ShellDialogLinux* ShellDialogLinux::instance() {
  return g_shell_dialog_linux;
}

void ShellDialogLinux::Initialize() {
  SelectFileDialogLinuxPortal::StartAvailabilityTestInBackground();
}

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
  if (dialog_choice_ == kUnknown) {
    // Start out assuming we are going to use dialogs from the toolkit.
    dialog_choice_ = kToolkit;

    // Check to see if the portal is available.
    if (SelectFileDialogLinuxPortal::IsPortalAvailable()) {
      dialog_choice_ = kPortal;
    } else {
      // Make sure to kill the portal connection.
      SelectFileDialogLinuxPortal::DestroyPortalConnection();

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
            dialog_choice_ = kKde;
          }
        }
      }
    }
  }

  const ui::ShellDialogLinux* shell_dialogs = ui::ShellDialogLinux::instance();
  switch (dialog_choice_) {
    case kToolkit:
      if (!shell_dialogs)
        break;
      return shell_dialogs->CreateSelectFileDialog(listener, std::move(policy));
    case kPortal:
      return new SelectFileDialogLinuxPortal(listener, std::move(policy));
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
