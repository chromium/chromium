// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_WOLVIC_WEB_CONTENTS_DELEGATE_H_
#define WOLVIC_BROWSER_WOLVIC_WEB_CONTENTS_DELEGATE_H_

#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"

namespace wolvic {

class WolvicJavascriptDialogManager;

class WolvicWebContentsDelegate
    : public web_contents_delegate_android::WebContentsDelegateAndroid {
 public:
  WolvicWebContentsDelegate(JNIEnv* env, jobject obj);
  ~WolvicWebContentsDelegate() override;

  void LoadingStateChanged(content::WebContents* source,
                           bool should_show_loading_ui) override;

  // See //android_webview/docs/how-does-on-create-window-work.md for more
  // details.
  void AddNewContents(content::WebContents* source,
                      std::unique_ptr<content::WebContents> new_contents,
                      const GURL& target_url,
                      WindowOpenDisposition disposition,
                      const blink::mojom::WindowFeatures& window_features,
                      bool user_gesture,
                      bool* was_blocked) final;

  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) final;

  std::unique_ptr<content::ColorChooser> OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) override;

 private:
  std::unique_ptr<content::WebContents> new_contents_;
  std::unique_ptr<WolvicJavascriptDialogManager> javascript_dialog_manager_;
};

}// namespace wolvic

#endif // WOLVIC_BROWSER_WOLVIC_WEB_CONTENTS_DELEGATE_H_
