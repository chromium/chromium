// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define UI_WEBUI_EXAMPLES_BROWSER_CONTENT_BROWSER_CLIENT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/public/browser/content_browser_client.h"

namespace webui_examples {

class BrowserMainParts;

class ContentBrowserClient : public content::ContentBrowserClient {
 public:
  ContentBrowserClient();
  ContentBrowserClient(const ContentBrowserClient&) = delete;
  ContentBrowserClient& operator=(const ContentBrowserClient&) = delete;
  ~ContentBrowserClient() override;

 private:
  // content::ContentBrowserClient:
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  std::unique_ptr<content::WebContentsViewDelegate> GetWebContentsViewDelegate(
      content::WebContents* web_contents) override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
  void RegisterAssociatedInterfaceBindersForRenderFrameHost(
      content::RenderFrameHost& render_frame_host,
      blink::AssociatedInterfaceRegistry& associated_registry) override;
  std::string GetUserAgent() override;

  raw_ptr<BrowserMainParts, AcrossTasksDanglingUntriaged> browser_main_parts_ =
      nullptr;
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_CONTENT_BROWSER_CLIENT_H_
