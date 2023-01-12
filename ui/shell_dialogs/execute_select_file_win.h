// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_EXECUTE_SELECT_FILE_WIN_H_
#define UI_SHELL_DIALOGS_EXECUTE_SELECT_FILE_WIN_H_

#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/win/windows_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace base {
class FilePath;
}

namespace ui {

// Describes a filter for a file dialog.
struct FileFilterSpec {
  // A human readable description of this filter. E.g. "HTML Files."
  std::u16string description;
  // The different extensions that map to this spec. This is a semicolon-
  // separated list of extensions that contains a wildcard and the separator.
  // E.g. "*.html;*.htm"
  std::u16string extension_spec;
};

using OnSelectFileExecutedCallback =
    base::OnceCallback<void(const std::vector<base::FilePath>&, int)>;

// Shows the file selection dialog modal to |owner| returns the selected file(s)
// and file type index using the |on_select_file_executed_callback|. The file
// path vector will be empty on failure.
SHELL_DIALOGS_EXPORT
void ExecuteSelectFile(
    SelectFileDialog::Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const std::vector<FileFilterSpec>& filter,
    int file_type_index,
    const std::wstring& default_extension,
    HWND owner,
    OnSelectFileExecutedCallback on_select_file_executed_callback);

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_EXECUTE_SELECT_FILE_WIN_H_
