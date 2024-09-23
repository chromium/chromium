// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog.h"

#include "base/notreached.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace ui {

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace ui
