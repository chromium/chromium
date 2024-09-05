// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_FAKE_SELECT_FILE_DIALOG_H_
#define UI_SHELL_DIALOGS_FAKE_SELECT_FILE_DIALOG_H_

#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/shell_dialogs_export.h"

namespace ui {

// A test fake SelectFileDialog. Usage:
//
// FakeSelectFileDialog::Factory* factory =
//   FakeSelectFileDialog::RegisterFactory();
// factory->SetOpenCallback(open_callback);
//
// Now calls to SelectFileDialog::Create() will create a |FakeSelectFileDialog|,
// and open_callback is invoked when the dialog is opened.
//
// Once the dialog is opened, use factory->GetLastDialog() to access the dialog
// to query file information and select a file.
class FakeSelectFileDialog : public SelectFileDialog {
 public:
  // A |FakeSelectFileDialog::Factory| which creates |FakeSelectFileDialog|
  // instances.
  class Factory : public SelectFileDialogFactory {
   public:
    Factory();
    ~Factory() override;
    Factory(const Factory&) = delete;
    Factory& operator=(const Factory&) = delete;

    // SelectFileDialog::Factory.
    ui::SelectFileDialog* Create(
        ui::SelectFileDialog::Listener* listener,
        std::unique_ptr<ui::SelectFilePolicy> policy) override;

    // Returns the last opened dialog, or null if one has not been opened yet.
    FakeSelectFileDialog* GetLastDialog() const;

    // Sets a callback to be called when a new fake dialog has been opened.
    void SetOpenCallback(base::RepeatingClosure callback);

   private:
    base::RepeatingClosure opened_callback_;
    base::WeakPtr<FakeSelectFileDialog> last_dialog_;
  };

  // Creates a |Factory| and registers it with |SelectFileDialog::SetFactory|,
  // which owns the new factory. This factory will create new
  // |FakeSelectFileDialog| instances upon calls to
  // |SelectFileDialog::Create()|.
  static Factory* RegisterFactory();

  FakeSelectFileDialog(const base::RepeatingClosure& opened,
                       Listener* listener,
                       std::unique_ptr<SelectFilePolicy> policy);
  FakeSelectFileDialog(const FakeSelectFileDialog&) = delete;
  FakeSelectFileDialog& operator=(const FakeSelectFileDialog&) = delete;

  // SelectFileDialog.
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override;
  bool HasMultipleFileTypeChoicesImpl() override;
  bool IsRunning(gfx::NativeWindow owning_window) const override;
  void ListenerDestroyed() override;

  // Returns the file title provided to the dialog.
  const std::u16string& title() const { return title_; }
  // Returns the file types provided to the dialog.
  const FileTypeInfo& file_types() const { return file_types_; }
  // Returns the default file extension provided to the dialog.
  const std::string& default_extension() const { return default_extension_; }
  // Returns the caller URL provided to the dialog.
  const GURL* caller() const { return caller_; }

  // Calls the |FileSelected()| method on listener(). |filter_text| selects
  // which file extension filter to report.
  [[nodiscard]] bool CallFileSelected(const base::FilePath& file_path,
                                      std::string_view filter_text);

  // Calls the |MultiFilesSelected()| method on listener().
  void CallMultiFilesSelected(const std::vector<base::FilePath>& file_path);

  // Calls the |FileSelectionCanceled()| method on listener().
  void CallFileSelectionCanceled();

  base::WeakPtr<FakeSelectFileDialog> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  ~FakeSelectFileDialog() override;

  base::RepeatingClosure opened_;
  std::u16string title_;
  FileTypeInfo file_types_;
  std::string default_extension_;
  raw_ptr<const GURL, DanglingUntriaged> caller_;
  base::WeakPtrFactory<FakeSelectFileDialog> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_FAKE_SELECT_FILE_DIALOG_H_
