// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_IOS_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_IOS_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

@class NativeFileDialog;

namespace ui {

// Implementation of SelectFileDialog that shows iOS dialogs for choosing a
// file or folder.
class SelectFileDialogImpl : public SelectFileDialog {
 public:
  SelectFileDialogImpl(Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy);

  SelectFileDialogImpl(const SelectFileDialogImpl&) = delete;
  SelectFileDialogImpl& operator=(const SelectFileDialogImpl&) = delete;

  // BaseShellDialog:
  bool IsRunning(gfx::NativeWindow parent_window) const override;
  void ListenerDestroyed() override;

  void FileWasSelected(bool is_multi,
                       bool was_cancelled,
                       const std::vector<base::FilePath>& files,
                       int index);

 protected:
  // SelectFileDialog:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override;
  bool HasMultipleFileTypeChoicesImpl() override;

 private:
  ~SelectFileDialogImpl() override;

  bool has_multiple_file_type_choices_ = false;
  NativeFileDialog* __strong native_file_dialog_;
  base::WeakPtrFactory<SelectFileDialogImpl> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_IOS_H_
