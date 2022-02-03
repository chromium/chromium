// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/shell_dialog_linux.h"

#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace {

ui::ShellDialogLinux* g_shell_dialog_linux = nullptr;

}  // namespace

namespace ui {

void ShellDialogLinux::SetInstance(ShellDialogLinux* instance) {
  g_shell_dialog_linux = instance;
}

const ShellDialogLinux* ShellDialogLinux::instance() {
  return g_shell_dialog_linux;
}

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  const ui::ShellDialogLinux* shell_dialogs = ui::ShellDialogLinux::instance();
  if (shell_dialogs)
    return shell_dialogs->CreateSelectFileDialog(listener, std::move(policy));
#endif
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace ui
