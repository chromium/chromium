// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "build/config/linux/dbus/buildflags.h"
#include "ui/linux/linux_ui.h"
#include "ui/shell_dialogs/select_file_dialog_linux.h"
#include "ui/shell_dialogs/select_file_policy.h"

#if BUILDFLAG(USE_DBUS)
#include "ui/shell_dialogs/select_file_dialog_linux_portal.h"
#endif

namespace ui {

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
#if BUILDFLAG(USE_DBUS)
  return new SelectFileDialogLinuxPortal(listener, std::move(policy));
#else
  const LinuxUi* linux_ui = LinuxUi::instance();
  if (linux_ui) {
    return linux_ui->CreateSelectFileDialog(listener, std::move(policy));
  }
  return nullptr;
#endif
}

}  // namespace ui
