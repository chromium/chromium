// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_AUTO_CLOSE_DIALOG_EVENT_HANDLER_WIN_H_
#define UI_SHELL_DIALOGS_AUTO_CLOSE_DIALOG_EVENT_HANDLER_WIN_H_

#include <shobjidl_core.h>
#include <wrl.h>
#include <wrl/client.h>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"

namespace ui {

// This IFileDialogEvents implementation ensures that the file dialog it is
// attached to will be closed if its owner is closed.
class AutoCloseDialogEventHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IFileDialogEvents> {
 public:
  explicit AutoCloseDialogEventHandler(HWND owner_window);

 private:
  ~AutoCloseDialogEventHandler() override;

  // Initializes the event hook to watch for the owner window closing.
  HRESULT Initialize(IFileDialog* file_dialog);

  // Invoked by the event hook whenever a window is destroyed.
  void OnWindowDestroyedNotification(HWND window);

  static void CALLBACK EventHookCallback(HWINEVENTHOOK handle,
                                         DWORD event,
                                         HWND hwnd,
                                         LONG id_object,
                                         LONG id_child,
                                         DWORD event_thread,
                                         DWORD event_time);

  // IFileDialogEvents:
  IFACEMETHODIMP OnTypeChange(IFileDialog* pfd) override;
  IFACEMETHODIMP OnFileOk(IFileDialog*) override;
  IFACEMETHODIMP OnFolderChange(IFileDialog*) override;
  IFACEMETHODIMP OnFolderChanging(IFileDialog*, IShellItem*) override;
  IFACEMETHODIMP OnSelectionChange(IFileDialog*) override;
  IFACEMETHODIMP OnShareViolation(IFileDialog*,
                                  IShellItem*,
                                  FDE_SHAREVIOLATION_RESPONSE*) override;
  IFACEMETHODIMP OnOverwrite(IFileDialog*,
                             IShellItem*,
                             FDE_OVERWRITE_RESPONSE*) override;

  // Used by the event hook to notify the handler when a window is destroyed.
  static raw_ptr<AutoCloseDialogEventHandler> instance_;

  // This is the owner window. When it closes, the dialog window also needs to
  // be closed.
  HWND owner_window_ = nullptr;

  // Indicates if `Initialize()` has been invoked. Used to prevent multiple
  // initializations.
  bool initialize_called_ = false;

  // The event hook handle. Used to uninstall the hook.
  HWINEVENTHOOK event_hook_ = nullptr;

  HWND dialog_window_ = nullptr;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_AUTO_CLOSE_DIALOG_EVENT_HANDLER_WIN_H_
