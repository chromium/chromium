// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/on_screen_keyboard_display_manager_tab_tip.h"

#include <shobjidl.h>
#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/win_util.h"
#include "ui/base/ime/virtual_keyboard_controller_observer.h"
#include "ui/base/win/hidden_window.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/dip_util.h"

namespace {

constexpr base::TimeDelta kCheckOSKDelay = base::Milliseconds(1000);
constexpr base::TimeDelta kDismissKeyboardRetryTimeout =
    base::Milliseconds(100);
constexpr int kDismissKeyboardMaxRetries = 5;

constexpr wchar_t kOSKClassName[] = L"IPTip_Main_Window";

constexpr wchar_t kWindows8OSKRegPath[] =
    L"Software\\Classes\\CLSID\\{054AAE20-4BEA-4347-8A35-64A533254A9D}"
    L"\\LocalServer32";

}  // namespace

namespace ui {

// This class provides functionality to detect when the on screen keyboard
// is displayed and move the main window up if it is obscured by the keyboard.
class OnScreenKeyboardDetector {
 public:
  OnScreenKeyboardDetector(
      OnScreenKeyboardDisplayManagerTabTip* display_manager);

  OnScreenKeyboardDetector(const OnScreenKeyboardDetector&) = delete;
  OnScreenKeyboardDetector& operator=(const OnScreenKeyboardDetector&) = delete;

  ~OnScreenKeyboardDetector();

  // Schedules a delayed task which detects if the on screen keyboard was
  // displayed.
  void DetectKeyboard(HWND main_window);

  // Dismisses the on screen keyboard. If a call to display the keyboard was
  // made, this function waits for the keyboard to become visible by retrying
  // upto a maximum of kDismissKeyboardMaxRetries.
  void DismissKeyboard();

  // Returns true if the osk is visible.
  static bool IsKeyboardVisible();

 private:
  // Returns the occluded rect in dips.
  gfx::Rect GetOccludedRect();

  // Executes as a task and detects if the on screen keyboard is displayed.
  // Once the keyboard is displayed it schedules the HideIfNecessary() task to
  // detect when the keyboard is or should be hidden.
  void CheckIfKeyboardVisible();

  // Executes as a task and detects if the keyboard was hidden or should be
  // hidden.
  void HideIfNecessary();

  // Notifies observers that the keyboard was displayed.
  // A recurring task HideIfNecessary() is started to detect when the OSK
  // disappears.
  void HandleKeyboardVisible(const gfx::Rect& occluded_rect);

  // Notifies observers that the keyboard was hidden.
  // The observer list is cleared out after this notification.
  void HandleKeyboardHidden();

  raw_ptr<OnScreenKeyboardDisplayManagerTabTip> display_manager_;

  // The main window which displays the on screen keyboard.
  HWND main_window_ = nullptr;

  // Tracks if the keyboard was displayed.
  bool osk_visible_notification_received_ = false;

  // Set to true if a call to DetectKeyboard() was made.
  bool keyboard_detect_requested_ = false;

  // Contains the number of attempts made to dismiss the keyboard. Please refer
  // to the DismissKeyboard() function for more information.
  int keyboard_dismiss_retry_count_ = 0;

  // Should be the last member in the class. Helps ensure that tasks spawned
  // by this class instance are canceled when it is destroyed.
  base::WeakPtrFactory<OnScreenKeyboardDetector> keyboard_detector_factory_{
      this};
};

// OnScreenKeyboardDetector member definitions.
OnScreenKeyboardDetector::OnScreenKeyboardDetector(
    OnScreenKeyboardDisplayManagerTabTip* display_manager)
    : display_manager_(display_manager) {}

OnScreenKeyboardDetector::~OnScreenKeyboardDetector() {}

void OnScreenKeyboardDetector::DetectKeyboard(HWND main_window) {
  main_window_ = main_window;
  keyboard_detect_requested_ = true;
  // The keyboard is displayed by TabTip.exe which is launched via a
  // ShellExecute call in the
  // OnScreenKeyboardDisplayManager::DisplayVirtualKeyboard() function. We use
  // a delayed task to check if the keyboard is visible because of the possible
  // delay between the ShellExecute call and the keyboard becoming visible.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OnScreenKeyboardDetector::CheckIfKeyboardVisible,
                     keyboard_detector_factory_.GetWeakPtr()),
      kCheckOSKDelay);
}

void OnScreenKeyboardDetector::DismissKeyboard() {
  // We dismiss the virtual keyboard by generating the SC_CLOSE.
  HWND osk = ::FindWindow(kOSKClassName, nullptr);
  if (::IsWindow(osk) && ::IsWindowEnabled(osk)) {
    keyboard_detect_requested_ = false;
    keyboard_dismiss_retry_count_ = 0;
    HandleKeyboardHidden();
    PostMessage(osk, WM_SYSCOMMAND, SC_CLOSE, 0);
    return;
  }

  if (keyboard_detect_requested_) {
    if (keyboard_dismiss_retry_count_ < kDismissKeyboardMaxRetries) {
      keyboard_dismiss_retry_count_++;
      // Please refer to the comments in the DetectKeyboard() function for more
      // information as to why we need a delayed task here.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              base::IgnoreResult(&OnScreenKeyboardDetector::DismissKeyboard),
              keyboard_detector_factory_.GetWeakPtr()),
          kDismissKeyboardRetryTimeout);
    } else {
      keyboard_dismiss_retry_count_ = 0;
    }
  }
}

// static
bool OnScreenKeyboardDetector::IsKeyboardVisible() {
  HWND osk = ::FindWindow(kOSKClassName, nullptr);
  if (!::IsWindow(osk))
    return false;
  return ::IsWindowVisible(osk) && ::IsWindowEnabled(osk);
}

gfx::Rect OnScreenKeyboardDetector::GetOccludedRect() {
  gfx::Rect occluded_rect;
  HWND osk = ::FindWindow(kOSKClassName, nullptr);
  if (!::IsWindow(osk) || !::IsWindowVisible(osk) || !::IsWindowEnabled(osk))
    return occluded_rect;

  RECT osk_rect = {};
  RECT main_window_rect = {};
  if (!::GetWindowRect(osk, &osk_rect) ||
      !::GetWindowRect(main_window_, &main_window_rect)) {
    return occluded_rect;
  }

  gfx::Rect gfx_osk_rect(osk_rect);
  gfx::Rect gfx_main_window_rect(main_window_rect);

  gfx_osk_rect.Intersect(gfx_main_window_rect);

  return display::win::ScreenWin::ScreenToDIPRect(main_window_, gfx_osk_rect);
}

void OnScreenKeyboardDetector::CheckIfKeyboardVisible() {
  gfx::Rect occluded_rect = GetOccludedRect();
  if (!occluded_rect.IsEmpty()) {
    if (!osk_visible_notification_received_)
      HandleKeyboardVisible(occluded_rect);
  } else {
    DVLOG(1) << "OSK did not come up. Something wrong.";
  }
}

void OnScreenKeyboardDetector::HideIfNecessary() {
  HWND osk = ::FindWindow(kOSKClassName, nullptr);
  if (!::IsWindow(osk))
    return;

  // Three cases here.
  // 1. OSK was hidden because the user dismissed it.
  // 2. We are no longer in the foreground.
  // 3. The OSK is still visible.
  // In the first case we just have to notify the observers that the OSK was
  // hidden.
  // In the second case we need to dismiss the OSK which internally will
  // notify the observers about the OSK being hidden.
  if (!::IsWindowEnabled(osk)) {
    if (osk_visible_notification_received_) {
      if (main_window_ == ::GetForegroundWindow()) {
        DVLOG(1) << "OSK window hidden while we are in the foreground.";
        HandleKeyboardHidden();
      }
    }
  } else if (main_window_ != ::GetForegroundWindow()) {
    if (osk_visible_notification_received_) {
      DVLOG(1) << "We are no longer in the foreground. Dismising OSK.";
      DismissKeyboard();
    }
  } else {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&OnScreenKeyboardDetector::HideIfNecessary,
                       keyboard_detector_factory_.GetWeakPtr()),
        kCheckOSKDelay);
  }
}

void OnScreenKeyboardDetector::HandleKeyboardVisible(
    const gfx::Rect& occluded_rect) {
  DCHECK(!osk_visible_notification_received_);
  osk_visible_notification_received_ = true;

  display_manager_->NotifyKeyboardVisible(occluded_rect);

  // Now that the keyboard is visible, run the task to detect if it was hidden.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OnScreenKeyboardDetector::HideIfNecessary,
                     keyboard_detector_factory_.GetWeakPtr()),
      kCheckOSKDelay);
}

void OnScreenKeyboardDetector::HandleKeyboardHidden() {
  osk_visible_notification_received_ = false;
  display_manager_->NotifyKeyboardHidden();
}

// OnScreenKeyboardDisplayManagerTabTip member definitions.
OnScreenKeyboardDisplayManagerTabTip::OnScreenKeyboardDisplayManagerTabTip(
    HWND hwnd)
    : hwnd_(hwnd) {}

OnScreenKeyboardDisplayManagerTabTip::~OnScreenKeyboardDisplayManagerTabTip() {}

bool OnScreenKeyboardDisplayManagerTabTip::DisplayVirtualKeyboard() {
  if (base::win::IsKeyboardPresentOnSlate(ui::GetHiddenWindow(), nullptr))
    return false;

  if (osk_path_.empty() && !GetOSKPath(&osk_path_)) {
    DLOG(WARNING) << "Failed to get on screen keyboard path from registry";
    return false;
  }

  HINSTANCE ret = ::ShellExecuteW(nullptr, L"", osk_path_.c_str(), nullptr,
                                  nullptr, SW_SHOW);

  bool success = reinterpret_cast<intptr_t>(ret) > 32;
  if (success) {
    // If multiple calls to DisplayVirtualKeyboard occur one after the other,
    // the last observer would be the one to get notifications.
    keyboard_detector_ = std::make_unique<OnScreenKeyboardDetector>(this);
    keyboard_detector_->DetectKeyboard(hwnd_);
  }
  return success;
}

void OnScreenKeyboardDisplayManagerTabTip::DismissVirtualKeyboard() {
  if (keyboard_detector_)
    keyboard_detector_->DismissKeyboard();
}

void OnScreenKeyboardDisplayManagerTabTip::AddObserver(
    VirtualKeyboardControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void OnScreenKeyboardDisplayManagerTabTip::RemoveObserver(
    VirtualKeyboardControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool OnScreenKeyboardDisplayManagerTabTip::GetOSKPath(std::wstring* osk_path) {
  DCHECK(osk_path);

  // We need to launch TabTip.exe from the location specified under the
  // LocalServer32 key for the {{054AAE20-4BEA-4347-8A35-64A533254A9D}}
  // CLSID.
  // TabTip.exe is typically found at
  // c:\program files\common files\microsoft shared\ink on English Windows.
  // We don't want to launch TabTip.exe from
  // c:\program files (x86)\common files\microsoft shared\ink. This path is
  // normally found on 64 bit Windows.
  base::win::RegKey key(HKEY_LOCAL_MACHINE, kWindows8OSKRegPath,
                        KEY_READ | KEY_WOW64_64KEY);
  DWORD osk_path_length = 1024;
  if (key.ReadValue(nullptr, base::WriteInto(osk_path, osk_path_length),
                    &osk_path_length, nullptr) != ERROR_SUCCESS) {
    return false;
  }

  osk_path->resize(wcslen(osk_path->c_str()));

  *osk_path = base::ToLowerASCII(*osk_path);

  size_t common_program_files_offset = osk_path->find(L"%commonprogramfiles%");
  // Typically the path to TabTip.exe read from the registry will start with
  // %CommonProgramFiles% which needs to be replaced with the corrsponding
  // expanded string.
  // If the path does not begin with %CommonProgramFiles% we use it as is.
  if (common_program_files_offset != std::wstring::npos) {
    // Preserve the beginning quote in the path.
    osk_path->erase(common_program_files_offset,
                    wcslen(L"%commonprogramfiles%"));
    // The path read from the registry contains the %CommonProgramFiles%
    // environment variable prefix. On 64 bit Windows the SHGetKnownFolderPath
    // function returns the common program files path with the X86 suffix for
    // the FOLDERID_ProgramFilesCommon value.
    // To get the correct path to TabTip.exe we first read the environment
    // variable CommonProgramW6432 which points to the desired common
    // files path. Failing that we fallback to the SHGetKnownFolderPath API.

    // We then replace the %CommonProgramFiles% value with the actual common
    // files path found in the process.
    std::wstring common_program_files_path;
    DWORD buffer_size =
        GetEnvironmentVariable(L"CommonProgramW6432", nullptr, 0);
    if (buffer_size) {
      GetEnvironmentVariable(
          L"CommonProgramW6432",
          base::WriteInto(&common_program_files_path, buffer_size),
          buffer_size);
      DCHECK(!common_program_files_path.empty());
    } else {
      base::win::ScopedCoMem<wchar_t> common_program_files;
      if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0, nullptr,
                                      &common_program_files))) {
        return false;
      }
      common_program_files_path = common_program_files;
    }
    osk_path->insert(common_program_files_offset, common_program_files_path);
  }
  return !osk_path->empty();
}

bool OnScreenKeyboardDisplayManagerTabTip::IsKeyboardVisible() {
  return OnScreenKeyboardDetector::IsKeyboardVisible();
}

void OnScreenKeyboardDisplayManagerTabTip::NotifyKeyboardVisible(
    const gfx::Rect& occluded_rect) {
  observers_.Notify(&VirtualKeyboardControllerObserver::OnKeyboardVisible,
                    occluded_rect);
}

void OnScreenKeyboardDisplayManagerTabTip::NotifyKeyboardHidden() {
  observers_.Notify(&VirtualKeyboardControllerObserver::OnKeyboardHidden);
}

}  // namespace ui
