// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements common select dialog functionality between GTK and KDE.

#include "ui/shell_dialogs/select_file_dialog_linux.h"

#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/threading/thread_restrictions.h"

namespace ui {

base::FilePath* SelectFileDialogLinux::last_saved_path_ = nullptr;
base::FilePath* SelectFileDialogLinux::last_opened_path_ = nullptr;

SelectFileDialogLinux::SelectFileDialogLinux(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : SelectFileDialog(listener, std::move(policy)) {
  if (!last_saved_path_) {
    last_saved_path_ = new base::FilePath();
    last_opened_path_ = new base::FilePath();
  }
}

SelectFileDialogLinux::~SelectFileDialogLinux() = default;

void SelectFileDialogLinux::ListenerDestroyed() {
  listener_ = nullptr;
}

bool SelectFileDialogLinux::CallDirectoryExistsOnUIThread(
    const base::FilePath& path) {
  base::ScopedAllowBlocking scoped_allow_blocking;
  return base::DirectoryExists(path);
}

}  // namespace ui
