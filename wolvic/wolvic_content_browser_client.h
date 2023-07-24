// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_WOLVIC_CONTENT_BROWSER_CLIENT_H_
#define WOLVIC_WOLVIC_CONTENT_BROWSER_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "wolvic/browser/vr/wolvic_xr_integration_client.h"

namespace content {

class BrowserContext;
class WolvicMainParts;

class WolvicContentBrowserClient : public ContentBrowserClient {
 public:
  explicit WolvicContentBrowserClient();

  WolvicContentBrowserClient(const WolvicContentBrowserClient&) = delete;
  WolvicContentBrowserClient& operator=(const WolvicContentBrowserClient&) =
      delete;

  ~WolvicContentBrowserClient() override;

  // Returns the single instance.
  static WolvicContentBrowserClient* Get();

  // Returns the single browser context for Wolvic.
  content::BrowserContext* GetBrowserContext();

  // ContentBrowserClient overrides.
  std::string GetUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  std::unique_ptr<BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
#if BUILDFLAG(ENABLE_VR)
  XrIntegrationClient* GetXrIntegrationClient() override;
#endif

 private:
  raw_ptr<WolvicMainParts> browser_main_parts_;
#if BUILDFLAG(ENABLE_VR)
  std::unique_ptr<wolvic::WolvicXrIntegrationClient> xr_integration_client_;
#endif
};

}  // namespace content

#endif  // WOLVIC_WOLVIC_CONTENT_BROWSER_CLIENT_H_
