// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements common select dialog functionality between GTK and KDE.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "base/nix/xdg_util.h"
#include "ui/aura/window.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace ui {

// Shared implementation SelectFileDialog used on Linux
class SHELL_DIALOGS_EXPORT SelectFileDialogLinux : public SelectFileDialog {
 public:
  SelectFileDialogLinux(const SelectFileDialogLinux&) = delete;
  SelectFileDialogLinux& operator=(const SelectFileDialogLinux&) = delete;

  // Returns true if the SelectFileDialog class returned by
  // NewSelectFileDialogImplKDE will actually work.
  static bool CheckKDEDialogWorksOnUIThread(std::string& kdialog_version);

  // BaseShellDialog implementation.
  void ListenerDestroyed() override;

 protected:
  explicit SelectFileDialogLinux(Listener* listener,
                                 std::unique_ptr<ui::SelectFilePolicy> policy);
  ~SelectFileDialogLinux() override;

  // SelectFileDialog implementation.
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override = 0;

  // Wrapper for base::DirectoryExists() that allow access on the UI
  // thread. Use this only in the file dialog functions, where it's ok
  // because the file dialog has to do many stats anyway. One more won't
  // hurt too badly and it's likely already cached.
  bool CallDirectoryExistsOnUIThread(const base::FilePath& path);

  const FileTypeInfo& file_types() const { return file_types_; }
  void set_file_types(const FileTypeInfo& file_types) {
    file_types_ = file_types;
  }

  size_t file_type_index() const { return file_type_index_; }
  void set_file_type_index(size_t file_type_index) {
    file_type_index_ = file_type_index;
  }

  Type type() const { return type_; }
  void set_type(Type type) { type_ = type; }

  static const base::FilePath* last_saved_path() { return last_saved_path_; }
  static void set_last_saved_path(const base::FilePath& last_saved_path) {
    *last_saved_path_ = last_saved_path;
  }

  static const base::FilePath* last_opened_path() { return last_opened_path_; }
  static void set_last_opened_path(const base::FilePath& last_opened_path) {
    *last_opened_path_ = last_opened_path;
  }

 private:
  // The file filters.
  FileTypeInfo file_types_;

  // The index of the default selected file filter.
  // Note: This starts from 1, not 0.
  size_t file_type_index_ = 0;

  // The type of dialog we are showing the user.
  Type type_ = SELECT_NONE;

  // These two variables track where the user last saved a file or opened a
  // file so that we can display future dialogs with the same starting path.
  static base::FilePath* last_saved_path_;
  static base::FilePath* last_opened_path_;
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LINUX_H_
