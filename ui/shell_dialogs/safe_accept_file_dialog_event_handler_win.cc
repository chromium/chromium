// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/safe_accept_file_dialog_event_handler_win.h"

#include <windows.h>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace ui {
namespace {

// Used by static hook callbacks to access the active handler instance.
SafeAcceptFileDialogEventHandler*& GetInstance() {
  static SafeAcceptFileDialogEventHandler* instance = nullptr;
  return instance;
}

// Windows API `GetKeyState()` bitmask indicating a virtual key is held down.
constexpr SHORT kKeyDownBitmask = 0x8000;

}  // namespace

SafeAcceptFileDialogEventHandler::SafeAcceptFileDialogEventHandler() {
  CHECK(!GetInstance());
  GetInstance() = this;
}

SafeAcceptFileDialogEventHandler::~SafeAcceptFileDialogEventHandler() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  CHECK(GetInstance());
  GetInstance() = nullptr;

  if (message_hook_) {
    ::UnhookWindowsHookEx(message_hook_);
  }
}

HRESULT SafeAcceptFileDialogEventHandler::OnTypeChange(
    IFileDialog* file_dialog) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // `OnTypeChange` will be called early in the dialog's lifecycle to detect if
  // the user is holding Enter as the dialog is presented. It will also be
  // invoked multiple times during the lifecycle of the dialog. Only do the
  // initialization once.
  if (initialize_called_) {
    return S_OK;
  }

  return Initialize(file_dialog);
}

HRESULT SafeAcceptFileDialogEventHandler::OnFileOk(IFileDialog*) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If acceptance occurs while Enter is physically up, accept immediately.
  if (!IsEnterKeyDown()) {
    OnEnterKeyboardReleased();
    return S_OK;
  }

  // Enforce a minimum display duration to prevent accidental immediate
  // acceptances. `AutofillSuggestionController` uses a 500ms delay to prevent
  // accidental interactions. That value is being reused here.
  // LINT.IfChange(MinDisplayDuration)
  constexpr base::TimeDelta kMinDisplayDuration = base::Milliseconds(500);
  // LINT.ThenChange(
  //     //chrome/browser/ui/autofill/autofill_suggestion_controller.h:IgnoreEarlyClicksDuration,
  //     //components/remote_cocoa/app_shim/select_file_dialog_bridge.mm:MinDisplayDuration
  // )
  if (base::TimeTicks::Now() - shown_time_ < kMinDisplayDuration) {
    return S_FALSE;
  }

  // If the user was holding Enter when the dialog opened, enforce that they
  // must physically release it before any acceptance can succeed.
  if (initial_enter_hold_) {
    return S_FALSE;
  }

  return S_OK;
}

HRESULT SafeAcceptFileDialogEventHandler::OnFolderChange(IFileDialog*) {
  return E_NOTIMPL;
}

HRESULT SafeAcceptFileDialogEventHandler::OnFolderChanging(IFileDialog*,
                                                           IShellItem*) {
  return E_NOTIMPL;
}

HRESULT SafeAcceptFileDialogEventHandler::OnSelectionChange(IFileDialog*) {
  return E_NOTIMPL;
}

HRESULT SafeAcceptFileDialogEventHandler::OnShareViolation(
    IFileDialog*,
    IShellItem*,
    FDE_SHAREVIOLATION_RESPONSE*) {
  return E_NOTIMPL;
}

HRESULT SafeAcceptFileDialogEventHandler::OnOverwrite(IFileDialog*,
                                                      IShellItem*,
                                                      FDE_OVERWRITE_RESPONSE*) {
  return E_NOTIMPL;
}

HRESULT SafeAcceptFileDialogEventHandler::Initialize(IFileDialog* file_dialog) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  CHECK(!initialize_called_);
  initialize_called_ = true;

  shown_time_ = base::TimeTicks::Now();

  // If the user is already holding Enter when the dialog is initialized,
  // track it to prevent keyjacking via hardware auto-repeat.
  if (IsEnterKeyDown()) {
    initial_enter_hold_ = true;
    CHECK(!message_hook_);
    message_hook_ = ::SetWindowsHookEx(WH_GETMESSAGE, &MessageHookCallback,
                                       nullptr, ::GetCurrentThreadId());
    base::UmaHistogramSparse("Windows.SetWindowsHookEx.Result",
                             message_hook_ ? 0 : ::GetLastError());
  }

  return S_OK;
}

bool SafeAcceptFileDialogEventHandler::IsEnterKeyDown() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (key_state_for_testing_.has_value()) {
    return key_state_for_testing_.value();
  }
  return (::GetKeyState(VK_RETURN) & kKeyDownBitmask) != 0;
}

void SafeAcceptFileDialogEventHandler::OnEnterKeyboardReleased() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Disarm the initial hold mitigation once Enter is physically released.
  if (initial_enter_hold_) {
    initial_enter_hold_ = false;
    if (message_hook_) {
      ::UnhookWindowsHookEx(message_hook_);
      message_hook_ = nullptr;
    }
  }
}

// static
LRESULT CALLBACK
SafeAcceptFileDialogEventHandler::MessageHookCallback(int code,
                                                      WPARAM wParam,
                                                      LPARAM lParam) {
  // Only intercept messages actively being removed from the queue
  // (`PM_REMOVE`), to avoid handling the same message multiple times during
  // `PM_NOREMOVE` peeking.
  if (code == HC_ACTION && wParam == PM_REMOVE) {
    auto* msg = reinterpret_cast<MSG*>(lParam);

    // Veto flag is cleared on physical KeyUp for Enter.
    if (msg->message == WM_KEYUP && msg->wParam == VK_RETURN) {
      if (GetInstance()) {
        GetInstance()->OnEnterKeyboardReleased();
      }
    }
  }
  return ::CallNextHookEx(nullptr, code, wParam, lParam);
}

}  // namespace ui
