// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_MAC_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_MAC_H_

#include <list>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/remote_cocoa/common/select_file_dialog.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace ui {

namespace test {
class SelectFileDialogMacTest;
}  // namespace test

// Implementation of SelectFileDialog that shows Cocoa dialogs for choosing a
// file or folder.
// Exported for unit tests.
class SHELL_DIALOGS_EXPORT SelectFileDialogImpl : public ui::SelectFileDialog {
 public:
  SelectFileDialogImpl(Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy);

  // BaseShellDialog implementation.
  bool IsRunning(gfx::NativeWindow parent_window) const override;
  void ListenerDestroyed() override;

 protected:
  // SelectFileDialog implementation.
  // |params| is user data we pass back via the Listener interface.
  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override;

 private:
  friend class test::SelectFileDialogMacTest;

  // Struct to store data associated with a file dialog while it is showing.
  struct DialogData {
    DialogData(gfx::NativeWindow parent_window_, void* params_);
    ~DialogData();

    // The parent window for the panel. Weak, used only for comparisons.
    gfx::NativeWindow parent_window;

    // |params| user data associated with this file dialog.
    void* params;

    // Bridge to the Cocoa NSSavePanel.
    mojo::Remote<remote_cocoa::mojom::SelectFileDialog> select_file_dialog;

   private:
    DISALLOW_COPY_AND_ASSIGN(DialogData);
  };

  ~SelectFileDialogImpl() override;

  // Callback made when a panel is closed.
  void FileWasSelected(DialogData* dialog_data,
                       bool is_multi,
                       bool was_cancelled,
                       const std::vector<base::FilePath>& files,
                       int index);

  bool HasMultipleFileTypeChoicesImpl() override;

  // A list containing a DialogData for all active dialogs.
  std::list<DialogData> dialog_data_list_;

  bool hasMultipleFileTypeChoices_;

  base::WeakPtrFactory<SelectFileDialogImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImpl);
};

}  // namespace ui

#endif  //  UI_SHELL_DIALOGS_SELECT_FILE_DIALOG_MAC_H_

