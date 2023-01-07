// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SHELL_DIALOG_LINUX_H_
#define UI_SHELL_DIALOGS_SHELL_DIALOG_LINUX_H_

#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace shell_dialog_linux {

// TODO(thomasanderson): Remove Initialize() and Finalize().

// Should be called before the first call to CreateSelectFileDialog.
SHELL_DIALOGS_EXPORT void Initialize();

SHELL_DIALOGS_EXPORT void Finalize();

}  // namespace shell_dialog_linux

#endif  // UI_SHELL_DIALOGS_SHELL_DIALOG_LINUX_H_
