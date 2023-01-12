// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_WIN_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_WIN_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/execute_select_file_win.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace base {
class FilePath;
}

namespace ui {

class SelectFilePolicy;
struct FileFilterSpec;

using ExecuteSelectFileCallback = base::RepeatingCallback<void(
    SelectFileDialog::Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const std::vector<FileFilterSpec>& filter,
    int file_type_index,
    const std::wstring& default_extension,
    HWND owner,
    OnSelectFileExecutedCallback on_select_file_executed_callback)>;

SHELL_DIALOGS_EXPORT SelectFileDialog* CreateWinSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy,
    const ExecuteSelectFileCallback& execute_select_file_callback);

}  // namespace ui

#endif  //  UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_WIN_H_

