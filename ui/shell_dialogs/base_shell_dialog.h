// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_BASE_SHELL_DIALOG_H_
#define UI_SHELL_DIALOGS_BASE_SHELL_DIALOG_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace ui {

// A base class for shell dialogs.
class SHELL_DIALOGS_EXPORT BaseShellDialog {
 public:
  // Returns true if a shell dialog box is currently being shown modally
  // to the specified owner.
  virtual bool IsRunning(gfx::NativeWindow owning_window) const = 0;

  // Notifies the dialog box that the listener has been destroyed and it should
  // no longer be sent notifications.
  virtual void ListenerDestroyed() = 0;

 protected:
  virtual ~BaseShellDialog();
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_BASE_SHELL_DIALOG_H_

