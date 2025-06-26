// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_WOLVIC_WEB_CONTENTS_DELEGATE_H_
#define WOLVIC_BROWSER_WOLVIC_WEB_CONTENTS_DELEGATE_H_

#include "base/scoped_multi_source_observation.h"
#include "components/embedder_support/android/delegate/web_contents_delegate_android.h"
#include "components/find_in_page/find_result_observer.h"
#include "components/find_in_page/find_tab_helper.h"

namespace wolvic {

class WolvicJavascriptDialogManager;

class WolvicWebContentsDelegate
    : public web_contents_delegate_android::WebContentsDelegateAndroid,
      public find_in_page::FindResultObserver {
 public:
  WolvicWebContentsDelegate(JNIEnv* env, jobject obj);
  ~WolvicWebContentsDelegate() override;

  void OnDidGetManifest(content::WebContents* web_contents,
                        const std::string& raw_manifest);

  // web_contents_delegate_android::WebContentsDelegateAndroid:
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
  bool ShouldResumeRequestsForCreatedWindow() override;

  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) final;

  std::unique_ptr<content::ColorChooser> OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) override;

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;

  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;

  void FindReply(content::WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;

  // find_in_page::FindResultObserver:
  void OnFindResultAvailable(content::WebContents* web_contents) override;
  void OnFindTabHelperDestroyed(find_in_page::FindTabHelper* helper) override;

 private:
  std::unique_ptr<content::WebContents> new_contents_;
  std::unique_ptr<WolvicJavascriptDialogManager> javascript_dialog_manager_;

  base::ScopedMultiSourceObservation<find_in_page::FindTabHelper,
                                     find_in_page::FindResultObserver>
      find_result_observations_{this};
};

}// namespace wolvic

#endif // WOLVIC_BROWSER_WOLVIC_WEB_CONTENTS_DELEGATE_H_
