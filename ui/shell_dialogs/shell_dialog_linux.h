// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SHELL_DIALOG_LINUX_H_
#define UI_SHELL_DIALOGS_SHELL_DIALOG_LINUX_H_

#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace ui {

// An interface that lets different Linux platforms override the
// CreateSelectFileDialog function declared here to return native file dialogs.
class SHELL_DIALOGS_EXPORT ShellDialogLinux {
 public:
  virtual ~ShellDialogLinux() {}

  // Sets the dynamically loaded singleton that draws the desktop native
  // UI. This pointer is not owned, and if this method is called a second time,
  // the first instance is not deleted.
  static void SetInstance(ShellDialogLinux* instance);

  // Returns a LinuxUI instance for the toolkit used in the user's desktop
  // environment.
  //
  // Can return NULL, in case no toolkit has been set. (For example, if we're
  // running with the "--ash" flag.)
  static const ShellDialogLinux* instance();

  // Returns a native file selection dialog.
  virtual SelectFileDialog* CreateSelectFileDialog(
      SelectFileDialog::Listener* listener,
      std::unique_ptr<SelectFilePolicy> policy) const = 0;
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SHELL_DIALOG_LINUX_H_
