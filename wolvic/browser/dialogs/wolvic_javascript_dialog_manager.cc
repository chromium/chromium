// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/dialogs/wolvic_javascript_dialog_manager.h"

#include "base/functional/callback.h"
#include "content/public/common/javascript_dialog_type.h"
#include "wolvic/browser/dialogs/user_dialog_manager_bridge.h"

namespace wolvic {

WolvicJavascriptDialogManager::WolvicJavascriptDialogManager() = default;
WolvicJavascriptDialogManager::~WolvicJavascriptDialogManager() = default;

void WolvicJavascriptDialogManager::RunJavaScriptDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  auto* bridge = UserDialogManagerBridge::GetInstance();
  *did_suppress_message = false;
  switch (dialog_type) {
    case content::JavaScriptDialogType::JAVASCRIPT_DIALOG_TYPE_ALERT:
      bridge->ShowAlertDialog(message_text, std::move(callback));
      break;
    case content::JavaScriptDialogType::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      bridge->ShowConfirmDialog(message_text, std::move(callback));
      break;
    case content::JavaScriptDialogType::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      bridge->ShowTextDialog(message_text, default_prompt_text,
                             std::move(callback));
      break;
    default:
      *did_suppress_message = true;
      // not supported...
  }
}

void WolvicJavascriptDialogManager::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  auto* bridge = UserDialogManagerBridge::GetInstance();
  bridge->ShowBeforeUnloadDialog(std::move(callback));
}

void WolvicJavascriptDialogManager::CancelDialogs(
    content::WebContents* web_contents,
    bool reset_state) {}

}  // namespace wolvic
