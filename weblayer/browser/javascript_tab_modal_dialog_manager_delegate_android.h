// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_JAVASCRIPT_TAB_MODAL_DIALOG_MANAGER_DELEGATE_ANDROID_H_
#define WEBLAYER_BROWSER_JAVASCRIPT_TAB_MODAL_DIALOG_MANAGER_DELEGATE_ANDROID_H_

#include "components/javascript_dialogs/tab_modal_dialog_manager_delegate.h"

namespace content {
class WebContents;
}

namespace weblayer {

class JavaScriptTabModalDialogManagerDelegateAndroid
    : public javascript_dialogs::TabModalDialogManagerDelegate {
 public:
  explicit JavaScriptTabModalDialogManagerDelegateAndroid(
      content::WebContents* web_contents);
  JavaScriptTabModalDialogManagerDelegateAndroid(
      const JavaScriptTabModalDialogManagerDelegateAndroid& other) = delete;
  JavaScriptTabModalDialogManagerDelegateAndroid& operator=(
      const JavaScriptTabModalDialogManagerDelegateAndroid& other) = delete;
  ~JavaScriptTabModalDialogManagerDelegateAndroid() override;

  // javascript_dialogs::TabModalDialogManagerDelegate
  base::WeakPtr<javascript_dialogs::TabModalDialogView> CreateNewDialog(
      content::WebContents* alerting_web_contents,
      const base::string16& title,
      content::JavaScriptDialogType dialog_type,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
      base::OnceClosure dialog_closed_callback) override;
  void WillRunDialog() override;
  void DidCloseDialog() override;
  void SetTabNeedsAttention(bool attention) override;
  bool IsWebContentsForemost() override;
  bool IsApp() override;

 private:
  content::WebContents* web_contents_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_JAVASCRIPT_TAB_MODAL_DIALOG_MANAGER_DELEGATE_ANDROID_H_
