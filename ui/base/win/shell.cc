// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/shell.h"

// clang-format off
#include <shlobj.h>  // Must be before propkey.
// clang-format on

#include <dwmapi.h>
#include <propkey.h>
#include <shellapi.h>
#include <wrl/client.h>

#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/native_library.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/shell_util.h"
#include "base/win/win_util.h"
#include "ui/base/ui_base_switches.h"

namespace ui::win {

BASE_FEATURE(kManuallyParsePathForShellExecute,
             base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

// If this feature is enabled, then the COM interface on explorer will be used
// to ShellExecute, rather than calling it directly.
BASE_FEATURE(kLaunchShellExecuteViaExplorer, base::FEATURE_DISABLED_BY_DEFAULT);

// Default ShellExecuteEx flags used with "openas", "explore", and default
// verbs.
//
// SEE_MASK_NOASYNC is specified so that ShellExecuteEx can be invoked from a
// thread whose message loop may not wait around long enough for the
// asynchronous tasks initiated by ShellExecuteEx to complete. Using this flag
// causes ShellExecuteEx() to block until these tasks complete.
const DWORD kDefaultShellExecuteFlags = SEE_MASK_NOASYNC;

bool InvokeShellExecuteDirectly(const std::wstring& path,
                                const std::wstring& working_directory,
                                const std::wstring& args,
                                const std::wstring& verb,
                                const std::wstring& class_name,
                                DWORD mask) {
  SHELLEXECUTEINFO sei = {};
  sei.cbSize = sizeof(sei);

  if (!class_name.empty()) {
    sei.lpClass = class_name.c_str();
    mask = (mask | SEE_MASK_CLASSNAME);
  }

  sei.fMask = mask;
  sei.nShow = SW_SHOWNORMAL;
  sei.lpVerb = (verb.empty() ? nullptr : verb.c_str());
  sei.lpDirectory =
      (working_directory.empty() ? nullptr : working_directory.c_str());
  sei.lpParameters = (args.empty() ? nullptr : args.c_str());

  base::win::ScopedCoMem<ITEMIDLIST_ABSOLUTE> path_id_list;
  if (base::FeatureList::IsEnabled(kManuallyParsePathForShellExecute)) {
    // ShellExecute will perform legacy resolution of a path if it can't detect
    // an extension from a given path, appending .pif, .com, .exe, .bat, .lnk,
    // and .cmd with an assumption the file is a truncated invocable path
    // (Example: "chrome" referring to "chrome.exe"). ShellExecute will perform
    // this resolution even if the path refers to a valid file. Chromium
    // expects paths to be fully qualified and does not need this resolution.
    if (FAILED(::SHParseDisplayName(path.c_str(), nullptr, &path_id_list,
                                    SFGAO_FILESYSTEM, nullptr))) {
      return false;
    }
    sei.fMask |= SEE_MASK_IDLIST;
    sei.lpIDList = path_id_list.get();
  } else {
    sei.lpFile = path.c_str();
  }

  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  return ::ShellExecuteExW(&sei);
}

// Invokes ShellExecuteExW() with the given parameters.
bool InvokeShellExecute(const std::wstring& path,
                        const std::wstring& working_directory,
                        const std::wstring& args,
                        const std::wstring& verb,
                        const std::wstring& class_name,
                        DWORD mask) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  // If the feature is disabled, or if complex parameters like class_name or
  // non-default masks are provided that RunShellExecuteViaExplorer doesn't
  // currently support, then call ShellExecute directly.
  if (!base::FeatureList::IsEnabled(kLaunchShellExecuteViaExplorer) ||
      !class_name.empty() || (mask & ~kDefaultShellExecuteFlags) != 0) {
    return InvokeShellExecuteDirectly(path, working_directory, args, verb,
                                      class_name, mask);
  }

  // ShellExecuteEx will perform legacy resolution of a path if it can't detect
  // an extension from a given path, appending .pif, .com, .exe, .bat, .lnk,
  // and .cmd with an assumption the file is a truncated invocable path
  // (Example: "chrome" referring to "chrome.exe"). ShellExecute will perform
  // this resolution even if the path refers to a valid file. Chromium
  // expects paths to be fully qualified and does not need this resolution.
  std::wstring resolved_path = path;
  if (base::FeatureList::IsEnabled(kManuallyParsePathForShellExecute)) {
    base::win::ScopedCoMem<ITEMIDLIST_ABSOLUTE> path_id_list;
    if (FAILED(::SHParseDisplayName(path.c_str(), nullptr, &path_id_list,
                                    SFGAO_FILESYSTEM, nullptr))) {
      return false;
    }

    wchar_t path_buffer[MAX_PATH];
    if (!::SHGetPathFromIDList(path_id_list, path_buffer)) {
      return false;
    }

    // The \\?\ prefix (the Win32 File Namespace) is used to tell the Windows
    // APIs to "disable all string parsing and to send the string that follows
    // it directly to the file system.". See
    // https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file#win32-file-namespace.
    resolved_path = base::StrCat({L"\\\\?\\", path_buffer});
  }

  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  base::win::ShellExecuteOptions options{
      .verb = verb, .current_directory = working_directory};
  return SUCCEEDED(
      base::win::RunShellExecuteViaExplorer(resolved_path, args, options));
}

}  // namespace

bool OpenFileViaShell(const base::FilePath& full_path) {
  // Invoke the default verb on the file with no arguments.
  return InvokeShellExecute(full_path.value(), full_path.DirName().value(),
                            std::wstring(), std::wstring(), std::wstring(),
                            kDefaultShellExecuteFlags);
}

bool OpenFolderViaShell(const base::FilePath& full_path) {
  // The "explore" verb causes the folder at |full_path| to be displayed in a
  // file browser. This will fail if |full_path| is not a directory.
  return InvokeShellExecute(full_path.value(), full_path.value(),
                            std::wstring(), L"explore", L"folder",
                            kDefaultShellExecuteFlags);
}

bool PreventWindowFromPinning(HWND hwnd) {
  DCHECK(hwnd);

  Microsoft::WRL::ComPtr<IPropertyStore> pps;
  if (FAILED(SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps))))
    return false;

  return base::win::SetBooleanValueForPropertyStore(
      pps.Get(), PKEY_AppUserModel_PreventPinning, true);
}

// TODO(calamity): investigate moving this out of the UI thread as COM
// operations may spawn nested run loops which can cause issues.
void SetAppDetailsForWindow(const std::wstring& app_id,
                            const base::FilePath& app_icon_path,
                            int app_icon_index,
                            const std::wstring& relaunch_command,
                            const std::wstring& relaunch_display_name,
                            HWND hwnd) {
  DCHECK(hwnd);

  Microsoft::WRL::ComPtr<IPropertyStore> pps;
  if (FAILED(SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps))))
    return;

  if (!app_id.empty())
    base::win::SetAppIdForPropertyStore(pps.Get(), app_id);
  if (!app_icon_path.empty()) {
    // Always add the icon index explicitly to prevent bad interaction with the
    // index notation when file path has commas.
    base::win::SetStringValueForPropertyStore(
        pps.Get(), PKEY_AppUserModel_RelaunchIconResource,
        base::StrCat({app_icon_path.value(), L",",
                      base::NumberToWString(app_icon_index)}));
  }
  if (!relaunch_command.empty()) {
    base::win::SetStringValueForPropertyStore(
        pps.Get(), PKEY_AppUserModel_RelaunchCommand, relaunch_command);
  }
  if (!relaunch_display_name.empty()) {
    base::win::SetStringValueForPropertyStore(
        pps.Get(), PKEY_AppUserModel_RelaunchDisplayNameResource,
        relaunch_display_name);
  }
}

void SetAppIdForWindow(const std::wstring& app_id, HWND hwnd) {
  SetAppDetailsForWindow(app_id, base::FilePath(), 0, std::wstring(),
                         std::wstring(), hwnd);
}

void SetAppIconForWindow(const base::FilePath& app_icon_path,
                         int app_icon_index,
                         HWND hwnd) {
  SetAppDetailsForWindow(std::wstring(), app_icon_path, app_icon_index,
                         std::wstring(), std::wstring(), hwnd);
}

void SetRelaunchDetailsForWindow(const std::wstring& relaunch_command,
                                 const std::wstring& display_name,
                                 HWND hwnd) {
  SetAppDetailsForWindow(std::wstring(), base::FilePath(), 0, relaunch_command,
                         display_name, hwnd);
}

void ClearWindowPropertyStore(HWND hwnd) {
  DCHECK(hwnd);

  Microsoft::WRL::ComPtr<IPropertyStore> pps;
  if (FAILED(SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&pps))))
    return;

  DWORD property_count;
  if (FAILED(pps->GetCount(&property_count)))
    return;

  PROPVARIANT empty_property_variant = {};
  for (DWORD i = property_count; i > 0; i--) {
    PROPERTYKEY key;
    if (SUCCEEDED(pps->GetAt(i - 1, &key))) {
      // Removes the value from |pps|'s array.
      pps->SetValue(key, empty_property_variant);
    }
  }
  if (FAILED(pps->Commit()))
    return;

  // Verify none of the keys are leaking.
  DCHECK(FAILED(pps->GetCount(&property_count)) || property_count == 0);
}

}  // namespace ui::win
