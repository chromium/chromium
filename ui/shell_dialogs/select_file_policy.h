// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_POLICY_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_POLICY_H_

#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace ui {

// An optional policy class that provides decisions on whether to allow showing
// a native file dialog. Some ports need this.
class SHELL_DIALOGS_EXPORT SelectFilePolicy {
 public:
  virtual ~SelectFilePolicy();

  // Returns true if the current policy allows for file selection dialogs.
  virtual bool CanOpenSelectFileDialog() = 0;

  // Called from the SelectFileDialog when we've denied a request.
  virtual void SelectFileDenied() = 0;
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_POLICY_H_

