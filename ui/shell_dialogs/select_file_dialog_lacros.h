// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LACROS_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LACROS_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chromeos/crosapi/mojom/select_file.mojom-forward.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace ui {

// SelectFileDialogLacros implements file open and save dialogs for the
// lacros-chrome binary. The dialog itself is handled by the file manager in
// ash-chrome.
class SHELL_DIALOGS_EXPORT SelectFileDialogLacros : public SelectFileDialog {
 public:
  class SHELL_DIALOGS_EXPORT Factory : public SelectFileDialogFactory {
   public:
    Factory();
    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;
    ~Factory() override;

    // SelectFileDialogFactory:
    ui::SelectFileDialog* Create(
        ui::SelectFileDialog::Listener* listener,
        std::unique_ptr<ui::SelectFilePolicy> policy) override;
  };

  SelectFileDialogLacros(Listener* listener,
                         std::unique_ptr<SelectFilePolicy> policy);
  SelectFileDialogLacros(const SelectFileDialogLacros&) = delete;
  SelectFileDialogLacros& operator=(const SelectFileDialogLacros&) = delete;

  // SelectFileDialog:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params,
                      const GURL* caller) override;
  bool HasMultipleFileTypeChoicesImpl() override;
  bool IsRunning(gfx::NativeWindow owning_window) const override;
  void ListenerDestroyed() override;

 private:
  // Private because SelectFileDialog is ref-counted.
  ~SelectFileDialogLacros() override;

  // Callback for file selection.
  void OnSelected(crosapi::mojom::SelectFileResult result,
                  std::vector<crosapi::mojom::SelectedFileInfoPtr> files,
                  int file_type_index);

  // Cached parameters from the call to SelectFileImpl.
  raw_ptr<void> params_ = nullptr;

  // The unique ID of the wayland shell surface that owns this dialog.
  std::string owning_shell_window_id_;
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_LACROS_H_
