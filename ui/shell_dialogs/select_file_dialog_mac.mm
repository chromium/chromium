// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog_mac.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "components/remote_cocoa/app_shim/select_file_dialog_bridge.h"
#include "components/remote_cocoa/browser/window.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/shell_dialogs/select_file_policy.h"

using remote_cocoa::mojom::SelectFileDialogType;
using remote_cocoa::mojom::SelectFileTypeInfo;
using remote_cocoa::mojom::SelectFileTypeInfoPtr;

namespace ui {

using Type = SelectFileDialog::Type;
using FileTypeInfo = SelectFileDialog::FileTypeInfo;

SelectFileDialogImpl::SelectFileDialogImpl(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : SelectFileDialog(listener, std::move(policy)), weak_factory_(this) {}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow parent_window) const {
  for (const auto& dialog_data : dialog_data_list_) {
    if (dialog_data.parent_window == parent_window)
      return true;
  }
  return false;
}

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = NULL;
}

void SelectFileDialogImpl::FileWasSelected(
    DialogData* dialog_data,
    bool is_multi,
    bool was_cancelled,
    const std::vector<base::FilePath>& files,
    int index) {
  auto it = std::find_if(
      dialog_data_list_.begin(), dialog_data_list_.end(),
      [dialog_data](const DialogData& d) { return &d == dialog_data; });
  DCHECK(it != dialog_data_list_.end());
  void* params = dialog_data->params;
  dialog_data_list_.erase(it);

  if (!listener_)
    return;

  if (was_cancelled || files.empty()) {
    listener_->FileSelectionCanceled(params);
  } else {
    if (is_multi) {
      listener_->MultiFilesSelected(files, params);
    } else {
      listener_->FileSelected(files[0], index, params);
    }
  }
}

void SelectFileDialogImpl::SelectFileImpl(
    Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow gfx_window,
    void* params) {
  DCHECK(type == SELECT_FOLDER || type == SELECT_UPLOAD_FOLDER ||
         type == SELECT_EXISTING_FOLDER || type == SELECT_OPEN_FILE ||
         type == SELECT_OPEN_MULTI_FILE || type == SELECT_SAVEAS_FILE);

  hasMultipleFileTypeChoices_ =
      file_types ? file_types->extensions.size() > 1 : true;

  // Add a new DialogData to the list. Note that it is safe to pass
  // |dialog_data| by pointer because it will only be removed from the list when
  // the callback is made or after the callback has been cancelled by
  // |weak_factory_|.
  dialog_data_list_.emplace_back(gfx_window, params);
  DialogData& dialog_data = dialog_data_list_.back();

  // Create a NSSavePanel for it.
  auto* mojo_window = remote_cocoa::GetWindowMojoInterface(gfx_window);
  auto receiver = dialog_data.select_file_dialog.BindNewPipeAndPassReceiver();
  if (mojo_window) {
    mojo_window->CreateSelectFileDialog(std::move(receiver));
  } else {
    NSWindow* ns_window = gfx_window.GetNativeNSWindow();
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<remote_cocoa::SelectFileDialogBridge>(ns_window),
        std::move(receiver));
  }

  // Show the panel.
  SelectFileDialogType mojo_type = SelectFileDialogType::kFolder;
  switch (type) {
    case SELECT_FOLDER:
      mojo_type = SelectFileDialogType::kFolder;
      break;
    case SELECT_UPLOAD_FOLDER:
      mojo_type = SelectFileDialogType::kUploadFolder;
      break;
    case SELECT_EXISTING_FOLDER:
      mojo_type = SelectFileDialogType::kExistingFolder;
      break;
    case SELECT_OPEN_FILE:
      mojo_type = SelectFileDialogType::kOpenFile;
      break;
    case SELECT_OPEN_MULTI_FILE:
      mojo_type = SelectFileDialogType::kOpenMultiFile;
      break;
    case SELECT_SAVEAS_FILE:
      mojo_type = SelectFileDialogType::kSaveAsFile;
      break;
    default:
      NOTREACHED();
      break;
  }

  SelectFileTypeInfoPtr mojo_file_types;
  if (file_types) {
    mojo_file_types = SelectFileTypeInfo::New();
    mojo_file_types->extensions = file_types->extensions;
    mojo_file_types->extension_description_overrides =
        file_types->extension_description_overrides;
    mojo_file_types->include_all_files = file_types->include_all_files;
  }

  auto callback = base::BindOnce(&SelectFileDialogImpl::FileWasSelected,
                                 weak_factory_.GetWeakPtr(), &dialog_data,
                                 type == SELECT_OPEN_MULTI_FILE);

  dialog_data.select_file_dialog->Show(
      mojo_type, title, default_path, std::move(mojo_file_types),
      file_type_index, default_extension, std::move(callback));
}

SelectFileDialogImpl::DialogData::DialogData(gfx::NativeWindow parent_window_,
                                             void* params_)
    : parent_window(parent_window_), params(params_) {}

SelectFileDialogImpl::DialogData::~DialogData() {}

SelectFileDialogImpl::~SelectFileDialogImpl() {
  // Clear |weak_factory_| beforehand, to ensure that no callbacks will be made
  // when we cancel the NSSavePanels.
  weak_factory_.InvalidateWeakPtrs();

  // Walk through the open dialogs and issue the cancel callbacks that would
  // have been made.
  for (const auto& dialog_data : dialog_data_list_) {
    if (listener_)
      listener_->FileSelectionCanceled(dialog_data.params);
  }

  // Cancel the NSSavePanels be destroying their bridges.
  dialog_data_list_.clear();
}

bool SelectFileDialogImpl::HasMultipleFileTypeChoicesImpl() {
  return hasMultipleFileTypeChoices_;
}

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
  return new SelectFileDialogImpl(listener, std::move(policy));
}

}  // namespace ui
