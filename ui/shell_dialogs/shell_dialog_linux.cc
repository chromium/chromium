// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/shell_dialog_linux.h"

#include "base/notreached.h"
#include "build/config/linux/dbus/buildflags.h"
#include "ui/linux/linux_ui.h"
#include "ui/shell_dialogs/select_file_dialog_linux.h"
#include "ui/shell_dialogs/select_file_policy.h"

#if BUILDFLAG(USE_DBUS)
#include "ui/shell_dialogs/select_file_dialog_linux_portal.h"
#endif

namespace shell_dialog_linux {

void Initialize() {
#if BUILDFLAG(USE_DBUS)
  ui::SelectFileDialogLinuxPortal::StartAvailabilityTestInBackground();
#endif
}

}  // namespace shell_dialog_linux

namespace ui {

namespace {

enum FileDialogChoice {
  kUnknown,
  kToolkit,
#if BUILDFLAG(USE_DBUS)
  kPortal,
#endif
};

FileDialogChoice dialog_choice_ = kUnknown;

FileDialogChoice GetFileDialogChoice() {
#if BUILDFLAG(USE_DBUS)
  // Check to see if the portal is available.
  if (SelectFileDialogLinuxPortal::IsPortalAvailable())
    return kPortal;
#endif

  return kToolkit;
}

}  // namespace

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
  if (dialog_choice_ == kUnknown)
    dialog_choice_ = GetFileDialogChoice();

  const LinuxUi* linux_ui = LinuxUi::instance();
  switch (dialog_choice_) {
    case kToolkit:
      if (!linux_ui)
        break;
      return linux_ui->CreateSelectFileDialog(listener, std::move(policy));
#if BUILDFLAG(USE_DBUS)
    case kPortal:
      return new SelectFileDialogLinuxPortal(listener, std::move(policy));
#endif
    case kUnknown:
      NOTREACHED();
  }
  return nullptr;
}

}  // namespace ui
