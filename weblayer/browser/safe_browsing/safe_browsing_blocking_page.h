// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_

#include <memory>

#include "components/safe_browsing/content/browser/base_blocking_page.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/unsafe_resource.h"

namespace content {
class WebContents;
}  // namespace content

namespace weblayer {
class SafeBrowsingUIManager;

class SafeBrowsingBlockingPage : public safe_browsing::BaseBlockingPage {
 public:
  typedef security_interstitials::UnsafeResource UnsafeResource;

  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  static SafeBrowsingBlockingPage* CreateBlockingPage(
      SafeBrowsingUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResource& unsafe_resource);

  // InterstitialPageDelegate methods:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

 private:
  SafeBrowsingBlockingPage(
      SafeBrowsingUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResourceList& unsafe_resources,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client,
      const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
