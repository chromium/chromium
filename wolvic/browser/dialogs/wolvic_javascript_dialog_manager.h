// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_DIALOGS_WOLVIC_JAVASCRIPT_DIALOG_MANAGER_H_
#define WOLVIC_BROWSER_DIALOGS_WOLVIC_JAVASCRIPT_DIALOG_MANAGER_H_

#include "content/public/browser/javascript_dialog_manager.h"

namespace wolvic {

class WolvicJavascriptDialogManager : public content::JavaScriptDialogManager {
 public:
  WolvicJavascriptDialogManager();
  WolvicJavascriptDialogManager(const WolvicJavascriptDialogManager&) = delete;
  WolvicJavascriptDialogManager& operator=(
      const WolvicJavascriptDialogManager&) = delete;
  ~WolvicJavascriptDialogManager() override;

  // JavaScriptDialogManager implementation.
  void RunJavaScriptDialog(content::WebContents* web_contents,
                           content::RenderFrameHost* render_frame_host,
                           content::JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             content::RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override;
  void CancelDialogs(content::WebContents* web_contents,
                     bool reset_state) override;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_DIALOGS_WOLVIC_JAVASCRIPT_DIALOG_MANAGER_H_
