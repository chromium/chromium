// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_KDE_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_KDE_H_

#include <string>

#include "base/nix/xdg_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace ui {

class SelectFileDialog;

SelectFileDialog* NewSelectFileDialogLinuxKde(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy,
    base::nix::DesktopEnvironment desktop,
    const std::string& kdialog_version);

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_KDE_H_
