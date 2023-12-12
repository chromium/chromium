// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_DIALOGS_USER_DIALOG_MANAGER_BRIDGE_H_
#define WOLVIC_BROWSER_DIALOGS_USER_DIALOG_MANAGER_BRIDGE_H_

#include "base/no_destructor.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace wolvic {

struct InProgressDialog;

using DialogCallback = content::JavaScriptDialogManager::DialogClosedCallback;

enum class DialogResult {
  kConfirmed,
  kDismissed,
};

// Implements a bridge used to forward dialog requests from Chromium to Wolvic.
class UserDialogManagerBridge {
 public:
  UserDialogManagerBridge(const UserDialogManagerBridge&) = delete;
  UserDialogManagerBridge& operator=(const UserDialogManagerBridge&) = delete;

  static UserDialogManagerBridge* GetInstance();

  void ShowAlertDialog(const std::u16string& message, DialogCallback callback);
  void ShowConfirmDialog(const std::u16string& message,
                         DialogCallback callback);
  void ShowTextDialog(const std::u16string& message,
                      const std::u16string& default_user_input,
                      DialogCallback callback);
  void ShowBeforeUnloadDialog(DialogCallback callback);

  void OnDialogClosed(InProgressDialog* dialog,
                      DialogResult result,
                      const std::u16string& user_input);

 private:
  UserDialogManagerBridge();
  ~UserDialogManagerBridge();

  friend base::NoDestructor<UserDialogManagerBridge>;

  std::vector<std::unique_ptr<InProgressDialog>> in_progress_dialogs_;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_DIALOGS_USER_DIALOG_MANAGER_BRIDGE_H_
