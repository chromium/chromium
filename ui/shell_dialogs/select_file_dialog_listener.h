// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LISTENER_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LISTENER_H_

#include <vector>

#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace ui {

struct SelectedFileInfo;

// An interface implemented by a Listener object wishing to know about the
// result of the Select File/Folder action. For any given call to
// SelectFile(), exactly one of these callbacks will be invoked once, possibly
// *but not necessarily* while SelectFile() is still on the stack.
class SHELL_DIALOGS_EXPORT SelectFileDialogListener {
 public:
  // Notifies the Listener that a file/folder selection has been made. The
  // file/folder path is in `file`. `index` specifies the index of the filter
  // passed to the initial call to SelectFile.
  virtual void FileSelected(const SelectedFileInfo& file, int index) {}

  // Notifies the Listener that many files have been selected. The files are
  // in `files`.
  // Implementing this method is optional if no multi-file selection is ever
  // made, as the default implementation will call NOTREACHED.
  virtual void MultiFilesSelected(const std::vector<SelectedFileInfo>& files);

  // Notifies the Listener that the file/folder selection was canceled (via
  // the user canceling or closing the selection dialog box, for example).
  virtual void FileSelectionCanceled() {}

 protected:
  virtual ~SelectFileDialogListener() = default;
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LISTENER_H_
