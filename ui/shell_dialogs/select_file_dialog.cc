// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog.h"

#include <stddef.h>
#include <algorithm>

#include "base/bind.h"
#include "base/check.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

// Optional dialog factory. Leaked.
ui::SelectFileDialogFactory* dialog_factory_ = NULL;

}  // namespace

namespace ui {

SelectFileDialog::FileTypeInfo::FileTypeInfo() = default;

SelectFileDialog::FileTypeInfo::FileTypeInfo(const FileTypeInfo& other) =
    default;

SelectFileDialog::FileTypeInfo::~FileTypeInfo() {}

void SelectFileDialog::Listener::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file,
    int index,
    void* params) {
  // Most of the dialogs need actual local path, so default to it.
  // If local path is empty, use file_path instead.
  FileSelected(file.local_path.empty() ? file.file_path : file.local_path,
               index, params);
}

void SelectFileDialog::Listener::MultiFilesSelectedWithExtraInfo(
    const std::vector<ui::SelectedFileInfo>& files,
    void* params) {
  std::vector<base::FilePath> file_paths;
  for (const ui::SelectedFileInfo& file : files) {
    file_paths.push_back(file.local_path.empty() ? file.file_path
                                                 : file.local_path);
  }

  MultiFilesSelected(file_paths, params);
}

// static
void SelectFileDialog::SetFactory(ui::SelectFileDialogFactory* factory) {
  delete dialog_factory_;
  dialog_factory_ = factory;
}

// static
scoped_refptr<SelectFileDialog> SelectFileDialog::Create(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  if (dialog_factory_)
    return dialog_factory_->Create(listener, std::move(policy));
  return CreateSelectFileDialog(listener, std::move(policy));
}

base::FilePath SelectFileDialog::GetShortenedFilePath(
    const base::FilePath& path) {
  const size_t kMaxNameLength = 255;
  if (path.BaseName().value().length() <= kMaxNameLength)
    return path;
  base::FilePath filename = path.BaseName();
  base::FilePath::StringType extension = filename.FinalExtension();
  filename = filename.RemoveFinalExtension();
  base::FilePath::StringType file_string = filename.value();
  // 1 for . plus 12 for longest known extension.
  size_t max_extension_length = 13;
  if (file_string.length() < kMaxNameLength) {
    max_extension_length =
        std::max(max_extension_length, kMaxNameLength - file_string.length());
  }
  if (extension.length() > max_extension_length) {
    // Take the first max_extension_length characters (this will be the
    // leading '.' plus the next max_extension_length - 1).
    extension.resize(max_extension_length);
  }
  file_string.resize(kMaxNameLength - extension.length());
  return path.DirName().Append(file_string).AddExtension(extension);
}

void SelectFileDialog::SelectFile(
    Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    void* params) {
  DCHECK(listener_);

  if (select_file_policy_.get() &&
      !select_file_policy_->CanOpenSelectFileDialog()) {
    select_file_policy_->SelectFileDenied();

    // Inform the listener that no file was selected.
    // Post a task rather than calling FileSelectionCanceled directly to ensure
    // that the listener is called asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&SelectFileDialog::CancelFileSelection, this, params));
    return;
  }

  base::FilePath path = GetShortenedFilePath(default_path);

  // Call the platform specific implementation of the file selection dialog.
  SelectFileImpl(type, title, path, file_types, file_type_index,
                 default_extension, owning_window, params);
}

bool SelectFileDialog::HasMultipleFileTypeChoices() {
  return HasMultipleFileTypeChoicesImpl();
}

SelectFileDialog::SelectFileDialog(Listener* listener,
                                   std::unique_ptr<ui::SelectFilePolicy> policy)
    : listener_(listener), select_file_policy_(std::move(policy)) {
  DCHECK(listener_);
}

SelectFileDialog::~SelectFileDialog() {}

void SelectFileDialog::CancelFileSelection(void* params) {
  if (listener_)
    listener_->FileSelectionCanceled(params);
}

}  // namespace ui
