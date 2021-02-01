// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/execute_select_file_win.h"

#include <shlobj.h>
#include <wrl/client.h>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/win/com_init_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/shortcut.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/base_shell_dialog_win.h"
#include "ui/strings/grit/ui_strings.h"

namespace ui {

namespace {

// Distinguish directories from regular files.
bool IsDirectory(const base::FilePath& path) {
  base::File::Info file_info;
  return base::GetFileInfo(path, &file_info) ? file_info.is_directory
                                             : path.EndsWithSeparator();
}

// Given |extension|, if it's not empty, then remove the leading dot.
std::wstring GetExtensionWithoutLeadingDot(const std::wstring& extension) {
  DCHECK(extension.empty() || extension[0] == L'.');
  return extension.empty() ? extension : extension.substr(1);
}

// Sets which path is going to be open when the dialog will be shown. If
// |default_path| is not only a directory, also sets the contents of the text
// box equals to the basename of the path.
bool SetDefaultPath(IFileDialog* file_dialog,
                    const base::FilePath& default_path) {
  if (default_path.empty())
    return true;

  base::FilePath default_folder;
  base::FilePath default_file_name;
  if (IsDirectory(default_path)) {
    default_folder = default_path;
  } else {
    default_folder = default_path.DirName();
    default_file_name = default_path.BaseName();
  }

  // Do not fail the file dialog operation if the specified folder is invalid.
  Microsoft::WRL::ComPtr<IShellItem> default_folder_shell_item;
  if (SUCCEEDED(SHCreateItemFromParsingName(
          default_folder.value().c_str(), nullptr,
          IID_PPV_ARGS(&default_folder_shell_item)))) {
    if (FAILED(file_dialog->SetFolder(default_folder_shell_item.Get())))
      return false;
  }

  return SUCCEEDED(file_dialog->SetFileName(default_file_name.value().c_str()));
}

// Sets the file extension filters on the dialog.
bool SetFilters(IFileDialog* file_dialog,
                const std::vector<FileFilterSpec>& filter,
                int filter_index) {
  if (filter.empty())
    return true;

  // A COMDLG_FILTERSPEC instance does not own any memory. |filter| must still
  // be alive at the time the dialog is shown.
  std::vector<COMDLG_FILTERSPEC> comdlg_filterspec(filter.size());

  for (size_t i = 0; i < filter.size(); ++i) {
    comdlg_filterspec[i].pszName = base::as_wcstr(filter[i].description);
    comdlg_filterspec[i].pszSpec = base::as_wcstr(filter[i].extension_spec);
  }

  return SUCCEEDED(file_dialog->SetFileTypes(comdlg_filterspec.size(),
                                             comdlg_filterspec.data())) &&
         SUCCEEDED(file_dialog->SetFileTypeIndex(filter_index));
}

// Sets the requested |dialog_options|, making sure to keep the default values
// when not overwritten.
bool SetOptions(IFileDialog* file_dialog, DWORD dialog_options) {
  // First retrieve the default options for a file dialog.
  DWORD options;
  if (FAILED(file_dialog->GetOptions(&options)))
    return false;

  options |= dialog_options;

  return SUCCEEDED(file_dialog->SetOptions(options));
}

// Configures a |file_dialog| object given the specified parameters.
bool ConfigureDialog(IFileDialog* file_dialog,
                     const base::string16& title,
                     const base::string16& ok_button_label,
                     const base::FilePath& default_path,
                     const std::vector<FileFilterSpec>& filter,
                     int filter_index,
                     DWORD dialog_options) {
  // Set title.
  if (!title.empty()) {
    if (FAILED(file_dialog->SetTitle(base::as_wcstr(title))))
      return false;
  }

  if (!ok_button_label.empty()) {
    if (FAILED(file_dialog->SetOkButtonLabel(base::as_wcstr(ok_button_label))))
      return false;
  }

  return SetDefaultPath(file_dialog, default_path) &&
         SetOptions(file_dialog, dialog_options) &&
         SetFilters(file_dialog, filter, filter_index);
}

// Prompt the user for location to save a file.
// Callers should provide the filter string, and also a filter index.
// The parameter |index| indicates the initial index of filter description and
// filter pattern for the dialog box. If |index| is zero or greater than the
// number of total filter types, the system uses the first filter in the
// |filter| buffer. |index| is used to specify the initial selected extension,
// and when done contains the extension the user chose. The parameter |path|
// returns the file name which contains the drive designator, path, file name,
// and extension of the user selected file name. |def_ext| is the default
// extension to give to the file if the user did not enter an extension.
bool RunSaveFileDialog(HWND owner,
                       const base::string16& title,
                       const base::FilePath& default_path,
                       const std::vector<FileFilterSpec>& filter,
                       DWORD dialog_options,
                       const std::wstring& def_ext,
                       int* filter_index,
                       base::FilePath* path) {
  Microsoft::WRL::ComPtr<IFileSaveDialog> file_save_dialog;
  if (FAILED(::CoCreateInstance(CLSID_FileSaveDialog, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&file_save_dialog)))) {
    return false;
  }

  if (!ConfigureDialog(file_save_dialog.Get(), title, base::string16(),
                       default_path, filter, *filter_index, dialog_options)) {
    return false;
  }

  file_save_dialog->SetDefaultExtension(def_ext.c_str());

  HRESULT hr = file_save_dialog->Show(owner);
  BaseShellDialogImpl::DisableOwner(owner);
  if (FAILED(hr))
    return false;

  UINT file_type_index;
  if (FAILED(file_save_dialog->GetFileTypeIndex(&file_type_index)))
    return false;

  *filter_index = static_cast<int>(file_type_index);

  Microsoft::WRL::ComPtr<IShellItem> result;
  if (FAILED(file_save_dialog->GetResult(&result)))
    return false;

  base::win::ScopedCoMem<wchar_t> display_name;
  if (FAILED(result->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING,
                                    &display_name))) {
    return false;
  }

  *path = base::FilePath(display_name.get());
  return true;
}

// Runs an Open file dialog box, with similar semantics for input parameters as
// RunSaveFileDialog.
bool RunOpenFileDialog(HWND owner,
                       const base::string16& title,
                       const base::string16& ok_button_label,
                       const base::FilePath& default_path,
                       const std::vector<FileFilterSpec>& filter,
                       DWORD dialog_options,
                       int* filter_index,
                       std::vector<base::FilePath>* paths) {
  Microsoft::WRL::ComPtr<IFileOpenDialog> file_open_dialog;
  if (FAILED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&file_open_dialog)))) {
    return false;
  }

  // The FOS_FORCEFILESYSTEM option ensures that if the user enters a URL in the
  // "File name" box, it will be downloaded locally and its new file path will
  // be returned by the dialog. This was a default option in the deprecated
  // GetOpenFileName API.
  dialog_options |= FOS_FORCEFILESYSTEM;

  if (!ConfigureDialog(file_open_dialog.Get(), title, ok_button_label,
                       default_path, filter, *filter_index, dialog_options)) {
    return false;
  }

  HRESULT hr = file_open_dialog->Show(owner);
  BaseShellDialogImpl::DisableOwner(owner);
  if (FAILED(hr))
    return false;

  UINT file_type_index;
  if (FAILED(file_open_dialog->GetFileTypeIndex(&file_type_index)))
    return false;

  *filter_index = static_cast<int>(file_type_index);

  Microsoft::WRL::ComPtr<IShellItemArray> selected_items;
  if (FAILED(file_open_dialog->GetResults(&selected_items)))
    return false;

  DWORD result_count;
  if (FAILED(selected_items->GetCount(&result_count)))
    return false;

  DCHECK(result_count == 1 || (dialog_options & FOS_ALLOWMULTISELECT));

  std::vector<base::FilePath> result(result_count);
  for (DWORD i = 0; i < result_count; ++i) {
    Microsoft::WRL::ComPtr<IShellItem> shell_item;
    if (FAILED(selected_items->GetItemAt(i, &shell_item)))
      return false;

    base::win::ScopedCoMem<wchar_t> display_name;
    if (FAILED(shell_item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING,
                                          &display_name))) {
      return false;
    }

    result[i] = base::FilePath(display_name.get());
  }

  // Only modify the out parameter if the enumeration didn't fail.
  *paths = std::move(result);
  return !paths->empty();
}

// Runs a Folder selection dialog box, passes back the selected folder in |path|
// and returns true if the user clicks OK. If the user cancels the dialog box
// the value in |path| is not modified and returns false. Run on the dialog
// thread.
bool ExecuteSelectFolder(HWND owner,
                         SelectFileDialog::Type type,
                         const base::string16& title,
                         const base::FilePath& default_path,
                         std::vector<base::FilePath>* paths) {
  DCHECK(paths);

  base::string16 new_title = title;
  if (new_title.empty() && type == SelectFileDialog::SELECT_UPLOAD_FOLDER) {
    // If it's for uploading don't use default dialog title to
    // make sure we clearly tell it's for uploading.
    new_title =
        l10n_util::GetStringUTF16(IDS_SELECT_UPLOAD_FOLDER_DIALOG_TITLE);
  }

  base::string16 ok_button_label;
  if (type == SelectFileDialog::SELECT_UPLOAD_FOLDER) {
    ok_button_label = l10n_util::GetStringUTF16(
        IDS_SELECT_UPLOAD_FOLDER_DIALOG_UPLOAD_BUTTON);
  }

  DWORD dialog_options = FOS_PICKFOLDERS;

  std::vector<FileFilterSpec> no_filter;
  int filter_index = 0;

  return RunOpenFileDialog(owner, new_title, ok_button_label, default_path,
                           no_filter, dialog_options, &filter_index, paths);
}

bool ExecuteSelectSingleFile(HWND owner,
                             const base::string16& title,
                             const base::FilePath& default_path,
                             const std::vector<FileFilterSpec>& filter,
                             int* filter_index,
                             std::vector<base::FilePath>* paths) {
  // Note: The title is not passed down for historical reasons.
  // TODO(pmonette): Figure out if it's a worthwhile improvement.
  return RunOpenFileDialog(owner, base::string16(), base::string16(),
                           default_path, filter, 0, filter_index, paths);
}

bool ExecuteSelectMultipleFile(HWND owner,
                               const base::string16& title,
                               const base::FilePath& default_path,
                               const std::vector<FileFilterSpec>& filter,
                               int* filter_index,
                               std::vector<base::FilePath>* paths) {
  DWORD dialog_options = FOS_ALLOWMULTISELECT;

  // Note: The title is not passed down for historical reasons.
  // TODO(pmonette): Figure out if it's a worthwhile improvement.
  return RunOpenFileDialog(owner, base::string16(), base::string16(),
                           default_path, filter, dialog_options, filter_index,
                           paths);
}

bool ExecuteSaveFile(HWND owner,
                     const base::FilePath& default_path,
                     const std::vector<FileFilterSpec>& filter,
                     const std::wstring& def_ext,
                     int* filter_index,
                     base::FilePath* path) {
  DCHECK(path);
  // Having an empty filter for a bad user experience. We should always
  // specify a filter when saving.
  DCHECK(!filter.empty());

  DWORD dialog_options = FOS_OVERWRITEPROMPT;

  // Note: The title is not passed down for historical reasons.
  // TODO(pmonette): Figure out if it's a worthwhile improvement.
  return RunSaveFileDialog(owner, base::string16(), default_path, filter,
                           dialog_options, def_ext, filter_index, path);
}

}  // namespace

// This function takes the output of a SaveAs dialog: a filename, a filter and
// the extension originally suggested to the user (shown in the dialog box) and
// returns back the filename with the appropriate extension appended. If the
// user requests an unknown extension and is not using the 'All files' filter,
// the suggested extension will be appended, otherwise we will leave the
// filename unmodified. |filename| should contain the filename selected in the
// SaveAs dialog box and may include the path, |filter_selected| should be
// '*.something', for example '*.*' or it can be blank (which is treated as
// *.*). |suggested_ext| should contain the extension without the dot (.) in
// front, for example 'jpg'.
std::wstring AppendExtensionIfNeeded(const std::wstring& filename,
                                     const std::wstring& filter_selected,
                                     const std::wstring& suggested_ext) {
  DCHECK(!filename.empty());
  std::wstring return_value = filename;

  // If we wanted a specific extension, but the user's filename deleted it or
  // changed it to something that the system doesn't understand, re-append.
  // Careful: Checking net::GetMimeTypeFromExtension() will only find
  // extensions with a known MIME type, which many "known" extensions on Windows
  // don't have.  So we check directly for the "known extension" registry key.
  std::wstring file_extension(
      GetExtensionWithoutLeadingDot(base::FilePath(filename).Extension()));
  std::wstring key(L"." + file_extension);
  if (!(filter_selected.empty() || filter_selected == L"*.*") &&
      !base::win::RegKey(HKEY_CLASSES_ROOT, key.c_str(), KEY_READ).Valid() &&
      file_extension != suggested_ext) {
    if (return_value.back() != L'.')
      return_value.append(L".");
    return_value.append(suggested_ext);
  }

  // Strip any trailing dots, which Windows doesn't allow.
  size_t index = return_value.find_last_not_of(L'.');
  if (index < return_value.size() - 1)
    return_value.resize(index + 1);

  return return_value;
}

void ExecuteSelectFile(
    SelectFileDialog::Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const std::vector<FileFilterSpec>& filter,
    int file_type_index,
    const std::wstring& default_extension,
    HWND owner,
    OnSelectFileExecutedCallback on_select_file_executed_callback) {
  base::win::AssertComInitialized();
  std::vector<base::FilePath> paths;
  switch (type) {
    case SelectFileDialog::SELECT_FOLDER:
    case SelectFileDialog::SELECT_UPLOAD_FOLDER:
    case SelectFileDialog::SELECT_EXISTING_FOLDER:
      ExecuteSelectFolder(owner, type, title, default_path, &paths);
      break;
    case SelectFileDialog::SELECT_SAVEAS_FILE: {
      base::FilePath path;
      if (ExecuteSaveFile(owner, default_path, filter, default_extension,
                          &file_type_index, &path)) {
        paths.push_back(std::move(path));
      }
      break;
    }
    case SelectFileDialog::SELECT_OPEN_FILE:
      ExecuteSelectSingleFile(owner, title, default_path, filter,
                              &file_type_index, &paths);
      break;
    case SelectFileDialog::SELECT_OPEN_MULTI_FILE:
      ExecuteSelectMultipleFile(owner, title, default_path, filter,
                                &file_type_index, &paths);
      break;
    case SelectFileDialog::SELECT_NONE:
      NOTREACHED();
  }

  std::move(on_select_file_executed_callback).Run(paths, file_type_index);
}

}  // namespace ui
