// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/auto_close_dialog_event_handler_win.h"

#include <windows.h>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"

namespace ui {

AutoCloseDialogEventHandler::AutoCloseDialogEventHandler(HWND owner_window)
    : owner_window_(owner_window) {
  CHECK(!instance_);
  instance_ = this;

  CHECK(owner_window_);
}

AutoCloseDialogEventHandler::~AutoCloseDialogEventHandler() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  CHECK(instance_);
  instance_ = nullptr;

  if (event_hook_) {
    ::UnhookWinEvent(event_hook_);
  }
}

HRESULT AutoCloseDialogEventHandler::Initialize(IFileDialog* file_dialog) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  CHECK(!initialize_called_);
  initialize_called_ = true;

  CHECK(!event_hook_);
  CHECK(!dialog_window_);

  Microsoft::WRL::ComPtr<IOleWindow> ole_window;
  HRESULT hr = file_dialog->QueryInterface(IID_PPV_ARGS(&ole_window));
  if (FAILED(hr)) {
    return hr;
  }

  HWND dialog_window;
  hr = ole_window->GetWindow(&dialog_window);
  if (FAILED(hr)) {
    return hr;
  }

  // Get the process id and the thread id of the owner window to limit the
  // scope of the event hook.
  DWORD process_id = 0;
  DWORD thread_id = ::GetWindowThreadProcessId(owner_window_, &process_id);
  if (!process_id || !thread_id) {
    return E_FAIL;
  }

  // `SetWinEventHook` is used to be notified when the owner window is closed.
  // See https://devblogs.microsoft.com/oldnewthing/20111026-00/?p=9263
  CHECK(!event_hook_);
  event_hook_ = ::SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY,
                                  nullptr, &EventHookCallback, process_id,
                                  thread_id, WINEVENT_OUTOFCONTEXT);
  if (!event_hook_) {
    return E_FAIL;
  }

  dialog_window_ = dialog_window;
  return S_OK;
}

void AutoCloseDialogEventHandler::OnWindowDestroyedNotification(HWND window) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Ignore unrelated notifications.
  if (window != owner_window_) {
    return;
  }

  // IFileDialog::Close() expects to be called from a IFileDialogEvents callback
  // so it can't be used here. Send WM_CLOSE instead.
  ::PostMessage(dialog_window_, WM_CLOSE, 0, 0);
}

// static
void CALLBACK
AutoCloseDialogEventHandler::EventHookCallback(HWINEVENTHOOK handle,
                                               DWORD event,
                                               HWND hwnd,
                                               LONG id_object,
                                               LONG id_child,
                                               DWORD event_thread,
                                               DWORD event_time) {
  CHECK(event == EVENT_OBJECT_DESTROY);

  // Only care about window objects.
  if (id_object != OBJID_WINDOW) {
    return;
  }

  // This is safe thread-wise because WINEVENT_OUTOFCONTEXT guarantee that the
  // hook callback will be invoked on the same thread that set the hook.
  CHECK(instance_);
  instance_->OnWindowDestroyedNotification(hwnd);
}

HRESULT AutoCloseDialogEventHandler::OnTypeChange(IFileDialog* file_dialog) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // OnTypeChange will be invoked multiple times during the lifecycle of the
  // dialog. Only do the initialization once.
  if (initialize_called_) {
    return S_OK;
  }

  return Initialize(file_dialog);
}

HRESULT AutoCloseDialogEventHandler::OnFileOk(IFileDialog*) {
  return E_NOTIMPL;
}
HRESULT AutoCloseDialogEventHandler::OnFolderChange(IFileDialog*) {
  return E_NOTIMPL;
}
HRESULT AutoCloseDialogEventHandler::OnFolderChanging(IFileDialog*,
                                                      IShellItem*) {
  return E_NOTIMPL;
}
HRESULT AutoCloseDialogEventHandler::OnSelectionChange(IFileDialog*) {
  return E_NOTIMPL;
}
HRESULT AutoCloseDialogEventHandler::OnShareViolation(
    IFileDialog*,
    IShellItem*,
    FDE_SHAREVIOLATION_RESPONSE*) {
  return E_NOTIMPL;
}
HRESULT AutoCloseDialogEventHandler::OnOverwrite(IFileDialog*,
                                                 IShellItem*,
                                                 FDE_OVERWRITE_RESPONSE*) {
  return E_NOTIMPL;
}

// static
raw_ptr<AutoCloseDialogEventHandler> AutoCloseDialogEventHandler::instance_ =
    nullptr;

}  // namespace ui
