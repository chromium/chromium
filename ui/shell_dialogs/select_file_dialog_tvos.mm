// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notimplemented.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace ui {

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
  // TODO(crbug.com/391914246): Implement the CreateSelectFileDialog when tvOS
  // actually requires the functionality.
  TVOS_NOT_YET_IMPLEMENTED();
  return nullptr;
}

}  // namespace ui
