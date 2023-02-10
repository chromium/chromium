// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/wolvic_content_browser_client.h"

#include "wolvic/wolvic_main_parts.h"

namespace content {

WolvicContentBrowserClient::WolvicContentBrowserClient()
    : browser_main_parts_(nullptr) {}

WolvicContentBrowserClient::~WolvicContentBrowserClient() {}

std::unique_ptr<BrowserMainParts>
WolvicContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  CHECK(!browser_main_parts_);
  browser_main_parts_ = new WolvicMainParts();
  return std::unique_ptr<BrowserMainParts>(browser_main_parts_);
}

WolvicBrowserContext* WolvicContentBrowserClient::browser_context() {
  return browser_main_parts_->browser_context();
}

}  // namespace content
