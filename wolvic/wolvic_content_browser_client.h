// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_WOLVIC_CONTENT_BROWSER_CLIENT_H_
#define WOLVIC_WOLVIC_CONTENT_BROWSER_CLIENT_H_

#include "content/public/browser/content_browser_client.h"
#include "wolvic/browser/vr/wolvic_xr_integration_client.h"

namespace content {

class WolvicMainParts;
class WolvicBrowserContext;

class WolvicContentBrowserClient : public ContentBrowserClient {
 public:
  explicit WolvicContentBrowserClient();

  WolvicContentBrowserClient(const WolvicContentBrowserClient&) = delete;
  WolvicContentBrowserClient& operator=(const WolvicContentBrowserClient&) =
      delete;

  std::string GetUserAgent() override;
  std::string GetUserAgentBasedOnPolicy(
      content::BrowserContext* context) override;
  std::string GetFullUserAgent() override;
  std::string GetReducedUserAgent() override;

  ~WolvicContentBrowserClient() override;

  // ContentBrowserClient overrides.
  std::unique_ptr<BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
#if BUILDFLAG(ENABLE_VR)
  XrIntegrationClient* GetXrIntegrationClient() override;
#endif

  WolvicBrowserContext* browser_context();

 private:
  WolvicMainParts* browser_main_parts_;
#if BUILDFLAG(ENABLE_VR)
  std::unique_ptr<wolvic::WolvicXrIntegrationClient> xr_integration_client_;
#endif
};

}  // namespace content

#endif  // WOLVIC_WOLVIC_CONTENT_BROWSER_CLIENT_H_
