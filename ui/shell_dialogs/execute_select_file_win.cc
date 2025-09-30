// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/shell_dialogs/execute_select_file_win.h"

#include <Windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <wrl/client.h>

#include <memory>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/threading/hang_watcher.h"
#include "base/win/com_init_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/shortcut.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/auto_close_dialog_event_handler_win.h"
#include "ui/shell_dialogs/base_shell_dialog_win.h"
#include "ui/shell_dialogs/select_file_utils_win.h"
#include "ui/strings/grit/ui_strings.h"

namespace ui {

namespace {

// Stop switch for the AutoCloseDialogEventHandler.
BASE_FEATURE(kAutoCloseFileDialogs,
             "AutoCloseFileDialogs",
             base::FEATURE_ENABLED_BY_DEFAULT);

// RAII wrapper around AutoCloseDialogEventHandler.
class ScopedAutoCloseDialogEventHandler {
 public:
  ScopedAutoCloseDialogEventHandler(HWND owner_window, IFileDialog* file_dialog)
      : file_dialog_(file_dialog) {
    CHECK(file_dialog_);

    if (!owner_window) {
      return;
    }

    if (!base::FeatureList::IsEnabled(kAutoCloseFileDialogs)) {
      return;
    }

    Microsoft::WRL::ComPtr<IFileDialogEvents> dialog_event_handler =
        Microsoft::WRL::Make<AutoCloseDialogEventHandler>(owner_window);
    if (!dialog_event_handler) {
      return;
    }

    file_dialog_->Advise(dialog_event_handler.Get(), &cookie_);
  }

  ~ScopedAutoCloseDialogEventHandler() {
    if (cookie_) {
      file_dialog_->Unadvise(cookie_);
    }
  }

 private:
  Microsoft::WRL::ComPtr<IFileDialog> file_dialog_;
  DWORD cookie_ = 0;
};

// Distinguish directories from regular files.
bool IsDirectory(const base::FilePath& path) {
  base::File::Info file_info;
  return base::GetFileInfo(path, &file_info) ? file_info.is_directory
                                             : path.EndsWithSeparator();
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
    std::wstring sanitized = RemoveEnvVarFromFileName<wchar_t>(
        default_path.BaseName().value(), std::wstring(L"%"));
    default_file_name = base::FilePath(sanitized);
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
                     const std::u16string& title,
                     const std::u16string& ok_button_label,
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

// Configures a |file_dialog| object given the specified parameters.
bool ConfigureDialog_Legacy(OPENFILENAME_NT4W* open_file_name,
                     const std::u16string& title,
					 const base::FilePath& default_path,
                     const std::vector<FileFilterSpec>& filter,
                     DWORD dialog_options) {
  std::u16string filter_buffer;
  open_file_name->lStructSize = sizeof(OPENFILENAME_NT4W);
  // Set title.
  if (!title.empty()) {
    open_file_name->lpstrTitle = (LPCWSTR)title.c_str();
  }
  
  if (dialog_options & FOS_ALLOWMULTISELECT) {
	open_file_name->Flags |= OFN_ALLOWMULTISELECT;
  }
  
  if (!default_path.empty()) {
        open_file_name->lpstrInitialDir = (LPCWSTR)default_path.value().c_str();
  }
  
  if (filter.empty())
    return true;

  open_file_name->lpstrFilter = NULL;
  filter_buffer.clear();

 for (const auto& filter_spec : filter) {
	std::u16string filter_str = filter_spec.description;
    filter_buffer.append(filter_str);
	filter_buffer.push_back(0);
	filter_str = filter_spec.extension_spec;
	filter_buffer.append(filter_str);
	filter_buffer.push_back(0);
  }
  filter_buffer.push_back(0);
  
  PWSTR c_filter_buffer = (PWSTR) ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WCHAR)*filter_buffer.length());
  
  for (size_t i = 0; i < filter_buffer.length(); i++)
  {
	  c_filter_buffer[i] = filter_buffer.at(i);
  }
  
  open_file_name->lpstrFilter = c_filter_buffer;
  
  return true;
}

struct SelectFolderDialogOptions {
  const wchar_t* default_path;
  bool is_upload;
};

int CALLBACK BrowseCallbackProc(HWND window,
                                UINT message,
                                LPARAM parameter,
                                LPARAM data) {
  if (message == BFFM_INITIALIZED) {
    SelectFolderDialogOptions* options =
        reinterpret_cast<SelectFolderDialogOptions*>(data);
    if (options->is_upload) {
      SendMessage(window, BFFM_SETOKTEXT, 0,
                  reinterpret_cast<LPARAM>(
                      l10n_util::GetStringUTF16(
                          IDS_SELECT_UPLOAD_FOLDER_DIALOG_UPLOAD_BUTTON)
                          .c_str()));
    }
    if (options->default_path) {
      SendMessage(window, BFFM_SETSELECTION, TRUE,
                  reinterpret_cast<LPARAM>(options->default_path));
    }
  }
  return 0;
}

// Runs a Folder selection dialog box, passes back the selected folder in |path|
// and returns true if the user clicks OK. If the user cancels the dialog box
// the value in |path| is not modified and returns false. Run on the dialog
// thread.
bool ExecuteFolder_Legacy(HWND owner,
                           const std::u16string& title,
                           const base::FilePath& default_path,
                           std::vector<base::FilePath>* paths) {
  base::FilePath result_path;
  base::win::AssertComInitialized();
  DCHECK(paths);
  std::u16string new_title = title;
  wchar_t dir_buffer[MAX_PATH + 1];

  bool result = false;
  BROWSEINFO browse_info = {};
  browse_info.hwndOwner = owner;
  browse_info.lpszTitle = (LPCWSTR)new_title.c_str();
  browse_info.pszDisplayName = dir_buffer;
  browse_info.ulFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;

  // If uploading or a default path was provided, update the BROWSEINFO
  // and set the callback function for the dialog so the strings can be set in
  // the callback.
  SelectFolderDialogOptions dialog_options = {};
  if (!default_path.empty())
    dialog_options.default_path = default_path.value().c_str();
  browse_info.ulFlags |= BIF_NONEWFOLDERBUTTON;
 
  if (dialog_options.default_path) {
    browse_info.lParam = reinterpret_cast<LPARAM>(&dialog_options);
    browse_info.lpfn = &BrowseCallbackProc;
  }

  LPITEMIDLIST list = SHBrowseForFolderW(&browse_info);
  BaseShellDialogImpl::DisableOwner(owner);
  if (list) {
    STRRET out_dir_buffer = {};
    out_dir_buffer.uType = STRRET_WSTR;
    Microsoft::WRL::ComPtr<IShellFolder> shell_folder;
    if (SUCCEEDED(SHGetDesktopFolder(&shell_folder))) {
      HRESULT hr = shell_folder->GetDisplayNameOf(list, SHGDN_FORPARSING,
                                                  &out_dir_buffer);
      if (SUCCEEDED(hr) && out_dir_buffer.uType == STRRET_WSTR) {
        paths->push_back(base::FilePath(out_dir_buffer.pOleStr));
        CoTaskMemFree(out_dir_buffer.pOleStr);
        result = true;
      } else {
        // Use old way if we don't get what we want.
        wchar_t old_out_dir_buffer[MAX_PATH + 1];
        if (SHGetPathFromIDList(list, old_out_dir_buffer)) {
          paths->push_back(base::FilePath(old_out_dir_buffer));
          result = true;
        }
      }

      // According to MSDN, Win2000 will not resolve shortcuts, so we do it
      // ourselves.
      base::win::ResolveShortcut(paths->at(0), &paths->at(0), nullptr);
	  
    }
    CoTaskMemFree(list);
  }
  return result;
}

// static
std::vector<std::tuple<std::u16string, std::u16string>>
GetFilters(const OPENFILENAME* openfilename) {
  std::vector<std::tuple<std::u16string, std::u16string>> filters;

  const char16_t* display_string = (char16_t*)openfilename->lpstrFilter;
  if (!display_string) {
    return filters;
  }

  while (*display_string) {
    const char16_t* display_string_end = display_string;
    while (*display_string_end)
      ++display_string_end;
    const char16_t* pattern = display_string_end + 1;
    const char16_t* pattern_end = pattern;
    while (*pattern_end)
      ++pattern_end;
    filters.push_back(
        std::make_tuple(std::u16string(display_string, display_string_end),
                  std::u16string(pattern, pattern_end)));
    display_string = pattern_end + 1;
  }

  return filters;
}

// Given |extension|, if it's not empty, then remove the leading dot.
std::wstring GetExtensionWithoutLeadingDot(const std::wstring& extension) {
  DCHECK(extension.empty() || extension[0] == L'.');
  return extension.empty() ? extension : extension.substr(1);
}

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
  LOG(ERROR) << filter_selected;
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
                       const std::u16string& title,
                       const base::FilePath& default_path,
                       const std::vector<FileFilterSpec>& filter,
                       DWORD dialog_options,
                       const std::wstring& def_ext,
                       int* filter_index,
                       base::FilePath* path) {
  Microsoft::WRL::ComPtr<IFileSaveDialog> file_save_dialog;
  bool use_legacy_dialogs = false;
  OPENFILENAME_NT4W open_file_name = {0};
  if (FAILED(::CoCreateInstance(CLSID_FileSaveDialog, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&file_save_dialog)))) {
    use_legacy_dialogs = true;
  }

  if (!use_legacy_dialogs) {
	  if (!ConfigureDialog(file_save_dialog.Get(), title, std::u16string(),
						   default_path, filter, *filter_index, dialog_options)) {
		return false;
	  }
	} else {
 	  ConfigureDialog_Legacy(&open_file_name, title, default_path, filter, dialog_options);
   }
  if (!use_legacy_dialogs) {
	  file_save_dialog->SetDefaultExtension(def_ext.c_str());
   
	  // This handler auto-closes the file dialog if its owner window is closed.
	  auto auto_close_dialog_event_handler =
		  std::make_unique<ScopedAutoCloseDialogEventHandler>(
			  owner, file_save_dialog.Get());
	  // Never consider the current scope as hung. The hang watching deadline (if
	  // any) is not valid since the user can take unbounded time to choose the
	  // file.
      base::HangWatcher::InvalidateActiveExpectations();
	  HRESULT hr = file_save_dialog->Show(owner);
	  BaseShellDialogImpl::DisableOwner(owner);

	  // Remove the event handler regardless of the return value of Show().
	  auto_close_dialog_event_handler = nullptr;

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
  } else {
	  base::HangWatcher::InvalidateActiveExpectations();
	  open_file_name.hwndOwner = owner;
	  open_file_name.Flags |= OFN_OVERWRITEPROMPT | OFN_EXPLORER |
							  OFN_ENABLESIZING | OFN_NOCHANGEDIR |
						      OFN_PATHMUSTEXIST;
      wchar_t filename_buffer [UNICODE_STRING_MAX_CHARS];
	  filename_buffer[0] = L'\0';
	  if (!default_path.empty()) {
		base::FilePath suggested_file_name;
		base::FilePath suggested_directory;
		if (IsDirectory(default_path)) {
		  suggested_directory = default_path;
		} else {
		  suggested_directory = default_path.DirName();
		  suggested_file_name = default_path.BaseName();
		  // If the default_path is a root directory, |suggested_file_name| will be
		  // '\', and the call to GetSaveFileName below will fail.
		  if (suggested_file_name.value() == L"\\")
			suggested_file_name.clear();
		}
		  open_file_name.lpstrInitialDir = NULL;

		  if (!suggested_directory.empty()) {
		  open_file_name.lpstrInitialDir = suggested_directory.value().c_str();

			  if (!suggested_file_name.empty()) {
			  // The filename is ignored if no initial directory is supplied.
				  base::wcslcpy(filename_buffer,
								suggested_file_name.value().c_str(),
								sizeof(filename_buffer));
			  }
		  }
	  }
	  open_file_name.lpstrFile = filename_buffer;
	  open_file_name.nMaxFile = UNICODE_STRING_MAX_CHARS;
      if (!filter.empty() && ((int)filter.size() - 1) < *filter_index) {
            *filter_index = filter.size() - 1;
          }
	 // open_file_name.lpstrFilter = 
	//	  filter.empty() ? nullptr : (LPCWSTR)filter.at(*filter_index).extension_spec.c_str();
	  open_file_name.nFilterIndex = *filter_index;
	  open_file_name.lpstrDefExt = &def_ext[0];

	  BOOL success = ::GetSaveFileNameW((OPENFILENAMEW*)&open_file_name);
	  BaseShellDialogImpl::DisableOwner(owner);
	  if (!success) {
		if(open_file_name.lpstrFilter) {
		  ::HeapFree(::GetProcessHeap(), 0, (LPVOID)open_file_name.lpstrFilter);
		  open_file_name.lpstrFilter = nullptr;
	    }
		return false;
	  }

	  // Return the user's choice.
	  //*path = base::FilePath(open_file_name.lpstrFile);
	  *filter_index = open_file_name.nFilterIndex;

	  // Figure out what filter got selected. The filter index is 1-based.
	  std::u16string filter_selected;
	  if (*filter_index > 0) {
		std::vector<std::tuple<std::u16string, std::u16string>> filters =
			GetFilters((OPENFILENAMEW*)&open_file_name);
		if (*filter_index > (long long)filters.size())
		  NOTREACHED() << "Invalid filter index.";
		else
		  filter_selected = std::get<1>(filters[*filter_index - 1]);
	  }

	  // Get the extension that was suggested to the user (when the Save As dialog
	  // was opened).
	  std::wstring suggested_ext = GetExtensionWithoutLeadingDot(default_path.Extension());
      LOG(ERROR) << suggested_ext;
	  LOG(ERROR) << filter_selected;
	  // If we can't get the extension from the default_path, we use the default
	  // extension passed in. This is to cover cases like when saving a web page,
	  // where we get passed in a name without an extension and a default extension
	  // along with it.
	  if (suggested_ext.empty())
		suggested_ext.append(def_ext);

	  *path = base::FilePath(
		  AppendExtensionIfNeeded(base::FilePath(open_file_name.lpstrFile).value(), std::wstring(filter_selected.begin(), filter_selected.end()), suggested_ext));
		  
	if(open_file_name.lpstrFilter) {
		::HeapFree(::GetProcessHeap(), 0, (LPVOID)open_file_name.lpstrFilter);
		open_file_name.lpstrFilter = nullptr;
	}
	  return true;	  
  }
}

// Runs an Open file dialog box, with similar semantics for input parameters as
// RunSaveFileDialog.
bool RunOpenFileDialog(HWND owner,
                       const std::u16string& title,
                       const std::u16string& ok_button_label,
                       const base::FilePath& default_path,
                       const std::vector<FileFilterSpec>& filter,
                       DWORD dialog_options,
                       int* filter_index,
                       std::vector<base::FilePath>* paths) {
  Microsoft::WRL::ComPtr<IFileOpenDialog> file_open_dialog;
  bool use_legacy_dialogs = false;
  OPENFILENAME_NT4W open_file_name = {0};
  if (FAILED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&file_open_dialog)))) {
    use_legacy_dialogs = true;
  }

  // The FOS_FORCEFILESYSTEM option ensures that if the user enters a URL in the
  // "File name" box, it will be downloaded locally and its new file path will
  // be returned by the dialog. This was a default option in the deprecated
  // GetOpenFileName API.
  dialog_options |= FOS_FORCEFILESYSTEM;

  if(!use_legacy_dialogs) {
	  if (!ConfigureDialog(file_open_dialog.Get(), title, ok_button_label,
						   default_path, filter, *filter_index, dialog_options)) {
		return false;
	  }
  } else {
	  if (dialog_options & FOS_PICKFOLDERS)
		  return ExecuteFolder_Legacy(owner, title, default_path, paths);
	  ConfigureDialog_Legacy(&open_file_name, title, default_path, filter, dialog_options);
  }

  if (!use_legacy_dialogs) {
          // This handler auto-closes the file dialog if its owner window is
          // closed.
      auto auto_close_dialog_event_handler =
              std::make_unique<ScopedAutoCloseDialogEventHandler>(
                  owner, file_open_dialog.Get());
  
	  // Never consider the current scope as hung. The hang watching deadline (if
	  // any) is not valid since the user can take unbounded time to choose the
	  // file.
	  base::HangWatcher::InvalidateActiveExpectations();

	  HRESULT hr = file_open_dialog->Show(owner);
	  BaseShellDialogImpl::DisableOwner(owner);

	  // Remove the event handler regardless of the return value of Show().
	  auto_close_dialog_event_handler = nullptr;

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
  } else {
	  base::HangWatcher::InvalidateActiveExpectations();
	  wchar_t filename_buffer [UNICODE_STRING_MAX_CHARS];
	  filename_buffer[0] = '\0';
	  open_file_name.hwndOwner = owner;
	  open_file_name.lpstrFile = filename_buffer;
	  open_file_name.nMaxFile = UNICODE_STRING_MAX_CHARS;
	  open_file_name.Flags |= OFN_EXPLORER | OFN_HIDEREADONLY;
	  bool success = ::GetOpenFileNameW((OPENFILENAMEW*)&open_file_name);
	  BaseShellDialogImpl::DisableOwner(owner);
	  
	  if(open_file_name.lpstrFilter) {
		  ::HeapFree(::GetProcessHeap(), 0, (LPVOID)open_file_name.lpstrFilter);
		  open_file_name.lpstrFilter = nullptr;
	  }
	  
	  if(success) {
		  base::FilePath directory;
          std::vector<base::FilePath> filenames;
		  const wchar_t* selection = open_file_name.lpstrFile;
		  // The return value of |open_file_name.lpstrFile| is dependent on the
		  // value of the Multi-Select flag within |open_file_name|. If the flag is
		  // not set the return value will be a single null-terminated wide string.
		  // If it is set it will be more than one null-terminated wide string, itself
		  // terminated by an empty null-terminated wide string.
		  if (open_file_name.Flags & OFN_ALLOWMULTISELECT) {
			while (*selection) {  // Empty string indicates end of list.
			  filenames.push_back(base::FilePath(selection));
			  // Skip over filename and null-terminator.
			  selection += filenames.back().value().length() + 1;
			}
		  } else {
			filenames.push_back(base::FilePath(selection));
		  }
		  if (filenames.size() == 1) {
			// When there is one file, it contains the path and filename.
			directory = filenames.at(0).DirName();
			filenames.at(0) = filenames.at(0).BaseName();
		  } else if (filenames.size() > 1) {
			// Otherwise, the first string is the path, and the remainder are
			// filenames.
			directory = filenames.at(0);
			filenames.erase(filenames.begin());
		  }
		 for (std::vector<base::FilePath>::iterator it = filenames.begin();
			   it != filenames.end(); ++it) {
			paths->push_back(directory.Append(*it));
		  }
	  }

	   return !paths->empty();
   }
}

// Runs a Folder selection dialog box, passes back the selected folder in |path|
// and returns true if the user clicks OK. If the user cancels the dialog box
// the value in |path| is not modified and returns false. Run on the dialog
// thread.
bool ExecuteSelectFolder(HWND owner,
                         SelectFileDialog::Type type,
                         const std::u16string& title,
                         const base::FilePath& default_path,
                         std::vector<base::FilePath>* paths) {
  DCHECK(paths);

  std::u16string new_title = title;
  if (new_title.empty() && type == SelectFileDialog::SELECT_UPLOAD_FOLDER) {
    // If it's for uploading don't use default dialog title to
    // make sure we clearly tell it's for uploading.
    new_title =
        l10n_util::GetStringUTF16(IDS_SELECT_UPLOAD_FOLDER_DIALOG_TITLE);
  }

  std::u16string ok_button_label;
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
                             const std::u16string& title,
                             const base::FilePath& default_path,
                             const std::vector<FileFilterSpec>& filter,
                             int* filter_index,
                             std::vector<base::FilePath>* paths) {
  return RunOpenFileDialog(owner, title, std::u16string(), default_path, filter,
                           0, filter_index, paths);
}

bool ExecuteSelectMultipleFile(HWND owner,
                               const std::u16string& title,
                               const base::FilePath& default_path,
                               const std::vector<FileFilterSpec>& filter,
                               int* filter_index,
                               std::vector<base::FilePath>* paths) {
  DWORD dialog_options = FOS_ALLOWMULTISELECT;
  return RunOpenFileDialog(owner, title, std::u16string(), default_path, filter,
                           dialog_options, filter_index, paths);
}

bool ExecuteSaveFile(HWND owner,
                     const std::u16string& title,
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

  return RunSaveFileDialog(owner, title, default_path, filter, dialog_options,
                           def_ext, filter_index, path);
}

}  // namespace

void ExecuteSelectFile(
    SelectFileDialog::Type type,
    const std::u16string& title,
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
      if (ExecuteSaveFile(owner, title, default_path, filter, default_extension,
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
