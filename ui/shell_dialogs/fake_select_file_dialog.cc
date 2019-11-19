// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/fake_select_file_dialog.h"

#include "base/logging.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace ui {

FakeSelectFileDialog::Factory::Factory() = default;
FakeSelectFileDialog::Factory::~Factory() = default;

ui::SelectFileDialog* FakeSelectFileDialog::Factory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  FakeSelectFileDialog* dialog =
      new FakeSelectFileDialog(opened_callback_, listener, std::move(policy));
  last_dialog_ = dialog->GetWeakPtr();
  return dialog;
}

FakeSelectFileDialog* FakeSelectFileDialog::Factory::GetLastDialog() const {
  return last_dialog_.get();
}

void FakeSelectFileDialog::Factory::SetOpenCallback(
    base::RepeatingClosure callback) {
  opened_callback_ = callback;
}

// static
FakeSelectFileDialog::Factory* FakeSelectFileDialog::RegisterFactory() {
  Factory* factory = new Factory;
  ui::SelectFileDialog::SetFactory(factory);
  return factory;
}

FakeSelectFileDialog::FakeSelectFileDialog(
    const base::RepeatingClosure& opened,
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : ui::SelectFileDialog(listener, std::move(policy)), opened_(opened) {}

FakeSelectFileDialog::~FakeSelectFileDialog() = default;

bool FakeSelectFileDialog::HasMultipleFileTypeChoicesImpl() {
  return true;
}

bool FakeSelectFileDialog::IsRunning(gfx::NativeWindow owning_window) const {
  return true;
}

void FakeSelectFileDialog::SelectFileImpl(
    Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    void* params) {
  title_ = title;
  params_ = params;
  if (file_types)
    file_types_ = *file_types;
  default_extension_ = base::FilePath(default_extension).MaybeAsASCII();
  opened_.Run();
}

bool FakeSelectFileDialog::CallFileSelected(const base::FilePath& file_path,
                                            base::StringPiece filter_text) {
  for (size_t index = 0; index < file_types_.extensions.size(); ++index) {
    for (const base::FilePath::StringType& ext :
         file_types_.extensions[index]) {
      if (base::FilePath(ext).MaybeAsASCII() == filter_text) {
        // FileSelected accepts a 1-based index.
        listener_->FileSelected(file_path, index + 1, params_);
        return true;
      }
    }
  }
  return false;
}

}  // namespace ui
