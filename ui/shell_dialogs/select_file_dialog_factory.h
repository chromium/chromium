// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_FACTORY_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_FACTORY_H_

#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace ui {

class SelectFilePolicy;

// Some chrome components want to create their own SelectFileDialog objects
// (for example, using an extension to provide the select file dialog needs to
// live in chrome/ due to the extension dependency.)
//
// They can implement a factory which creates their SelectFileDialog.
class SHELL_DIALOGS_EXPORT SelectFileDialogFactory {
 public:
  virtual ~SelectFileDialogFactory();

  virtual SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) = 0;
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_FACTORY_H_
