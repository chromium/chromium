// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/examples/browser/content_browser_client.h"

#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "ui/webui/examples/browser/browser_main_parts.h"

namespace webui_examples {

ContentBrowserClient::ContentBrowserClient() = default;

ContentBrowserClient::~ContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
ContentBrowserClient::CreateBrowserMainParts(bool is_integration_test) {
  auto browser_main_parts = BrowserMainParts::Create();
  browser_main_parts_ = browser_main_parts.get();
  return browser_main_parts;
}

std::unique_ptr<content::WebContentsViewDelegate>
ContentBrowserClient::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return browser_main_parts_->CreateWebContentsViewDelegate(web_contents);
}

std::unique_ptr<content::DevToolsManagerDelegate>
ContentBrowserClient::CreateDevToolsManagerDelegate() {
  return browser_main_parts_->CreateDevToolsManagerDelegate();
}

}  // namespace webui_examples
