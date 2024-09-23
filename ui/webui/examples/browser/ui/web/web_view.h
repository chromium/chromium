// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_WEB_VIEW_H_
#define UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_WEB_VIEW_H_

#include "base/types/pass_key.h"
#include "components/guest_view/browser/guest_view.h"
#include "components/guest_view/browser/guest_view_base.h"

namespace webui_examples {

// A content embedding mechanism derived from the extensions <webview> tag
// that does not implement any of the extensions API.
class WebView : public guest_view::GuestView<WebView> {
 public:
  using PassKey = base::PassKey<WebView>;
  static constexpr char Type[] = "BrowserWebView";
  static const guest_view::GuestViewHistogramValue HistogramValue =
      guest_view::GuestViewHistogramValue::kInvalid;

  static std::unique_ptr<guest_view::GuestViewBase> Create(
      content::RenderFrameHost* owner_render_frame_host);

  WebView(PassKey pass_key, content::RenderFrameHost* owner_render_frame_host);
  WebView(const WebView&) = delete;
  WebView& operator=(const WebView&) = delete;
  ~WebView() override;

 private:
  // guest_view::GuestView<WebView>:
  const char* GetAPINamespace() const override;
  int GetTaskPrefix() const override;
  void CreateWebContents(std::unique_ptr<GuestViewBase> owned_this,
                         const base::Value::Dict& create_params,
                         WebContentsCreatedCallback callback) override;
  void MaybeRecreateGuestContents(
      content::RenderFrameHost* outer_contents_frame) override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) final;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_WEB_VIEW_H_
