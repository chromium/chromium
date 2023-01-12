// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog_lacros.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chromeos/crosapi/mojom/select_file.mojom-shared.h"
#include "chromeos/crosapi/mojom/select_file.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

namespace ui {
namespace {

crosapi::mojom::SelectFileDialogType GetMojoType(SelectFileDialog::Type type) {
  switch (type) {
    case SelectFileDialog::Type::SELECT_FOLDER:
      return crosapi::mojom::SelectFileDialogType::kFolder;
    case SelectFileDialog::Type::SELECT_UPLOAD_FOLDER:
      return crosapi::mojom::SelectFileDialogType::kUploadFolder;
    case SelectFileDialog::Type::SELECT_EXISTING_FOLDER:
      return crosapi::mojom::SelectFileDialogType::kExistingFolder;
    case SelectFileDialog::Type::SELECT_OPEN_FILE:
      return crosapi::mojom::SelectFileDialogType::kOpenFile;
    case SelectFileDialog::Type::SELECT_OPEN_MULTI_FILE:
      return crosapi::mojom::SelectFileDialogType::kOpenMultiFile;
    case SelectFileDialog::Type::SELECT_SAVEAS_FILE:
      return crosapi::mojom::SelectFileDialogType::kSaveAsFile;
    case SelectFileDialog::Type::SELECT_NONE:
      NOTREACHED();
      return crosapi::mojom::SelectFileDialogType::kOpenFile;
  }
}

crosapi::mojom::AllowedPaths GetMojoAllowedPaths(
    SelectFileDialog::FileTypeInfo::AllowedPaths allowed_paths) {
  switch (allowed_paths) {
    case SelectFileDialog::FileTypeInfo::ANY_PATH:
      return crosapi::mojom::AllowedPaths::kAnyPath;
    case SelectFileDialog::FileTypeInfo::NATIVE_PATH:
      return crosapi::mojom::AllowedPaths::kNativePath;
    case SelectFileDialog::FileTypeInfo::ANY_PATH_OR_URL:
      return crosapi::mojom::AllowedPaths::kAnyPathOrUrl;
  }
}

SelectedFileInfo ConvertSelectedFileInfo(
    crosapi::mojom::SelectedFileInfoPtr mojo_file) {
  SelectedFileInfo file;
  file.file_path = std::move(mojo_file->file_path);
  file.local_path = std::move(mojo_file->local_path);
  file.display_name = std::move(mojo_file->display_name);
  file.url = std::move(mojo_file->url);
  return file;
}

// Returns the ID of the Wayland shell surface that contains `window`, or an
// empty string if `window` is not associated with a top-level window.
std::string GetShellWindowUniqueId(aura::Window* window) {
  DCHECK(window);
  // If the window is not associated with a root window, there's no top-level
  // window to use as a parent for the file picker. Return an empty ID so
  // ash-chrome will use a modeless dialog.
  aura::Window* root_window = window->GetRootWindow();
  if (!root_window)
    return std::string();
  // On desktop aura there is one WindowTreeHost per top-level window.
  aura::WindowTreeHost* window_tree_host = root_window->GetHost();
  DCHECK(window_tree_host);

  return window_tree_host->GetUniqueId();
}

}  // namespace

SelectFileDialogLacros::Factory::Factory() = default;
SelectFileDialogLacros::Factory::~Factory() = default;

ui::SelectFileDialog* SelectFileDialogLacros::Factory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return new SelectFileDialogLacros(listener, std::move(policy));
}

SelectFileDialogLacros::SelectFileDialogLacros(
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : ui::SelectFileDialog(listener, std::move(policy)) {}

SelectFileDialogLacros::~SelectFileDialogLacros() = default;

bool SelectFileDialogLacros::HasMultipleFileTypeChoicesImpl() {
  return true;
}

bool SelectFileDialogLacros::IsRunning(gfx::NativeWindow owning_window) const {
  return !owning_shell_window_id_.empty() &&
         GetShellWindowUniqueId(owning_window) == owning_shell_window_id_;
}

void SelectFileDialogLacros::ListenerDestroyed() {
  listener_ = nullptr;
}

void SelectFileDialogLacros::SelectFileImpl(
    Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    void* params,
    const GURL* caller) {
  params_ = params;

  crosapi::mojom::SelectFileOptionsPtr options =
      crosapi::mojom::SelectFileOptions::New();
  options->type = GetMojoType(type);
  options->title = title;
  options->default_path = default_path;
  if (file_types) {
    options->file_types = crosapi::mojom::SelectFileTypeInfo::New();
    options->file_types->extensions = file_types->extensions;
    options->file_types->extension_description_overrides =
        file_types->extension_description_overrides;
    // NOTE: Index is 1-based, 0 means "no selection".
    options->file_types->default_file_type_index = file_type_index;
    options->file_types->include_all_files = file_types->include_all_files;
    options->file_types->allowed_paths =
        GetMojoAllowedPaths(file_types->allowed_paths);
  }
  // Modeless file dialogs have no owning window.
  if (owning_window) {
    owning_shell_window_id_ = GetShellWindowUniqueId(owning_window);
    options->owning_shell_window_id = owning_shell_window_id_;
  }
  if (caller && caller->is_valid()) {
    options->caller = *caller;
  }

  // Send request to ash-chrome.
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::SelectFile>()
      ->Select(std::move(options),
               base::BindOnce(&SelectFileDialogLacros::OnSelected, this));
}

void SelectFileDialogLacros::OnSelected(
    crosapi::mojom::SelectFileResult result,
    std::vector<crosapi::mojom::SelectedFileInfoPtr> mojo_files,
    int file_type_index) {
  owning_shell_window_id_.clear();
  if (!listener_)
    return;
  if (mojo_files.empty()) {
    listener_->FileSelectionCanceled(params_);
    return;
  }
  if (mojo_files.size() == 1) {
    SelectedFileInfo file = ConvertSelectedFileInfo(std::move(mojo_files[0]));
    listener_->FileSelectedWithExtraInfo(file, file_type_index, params_);
    return;
  }
  std::vector<SelectedFileInfo> files;
  for (auto& mojo_file : mojo_files) {
    files.push_back(ConvertSelectedFileInfo(std::move(mojo_file)));
  }
  listener_->MultiFilesSelectedWithExtraInfo(files, params_);
}

}  // namespace ui
