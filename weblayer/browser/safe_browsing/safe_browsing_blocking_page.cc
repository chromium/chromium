// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_blocking_page.h"

#include "base/metrics/histogram_macros.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "weblayer/browser/safe_browsing/safe_browsing_ui_manager.h"

namespace weblayer {

// static
const security_interstitials::SecurityInterstitialPage::TypeID
    SafeBrowsingBlockingPage::kTypeForTesting =
        &SafeBrowsingBlockingPage::kTypeForTesting;

SafeBrowsingBlockingPage::SafeBrowsingBlockingPage(
    SafeBrowsingUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResourceList& unsafe_resources,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client,
    const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options)
    : BaseBlockingPage(ui_manager,
                       web_contents,
                       main_frame_url,
                       unsafe_resources,
                       std::move(controller_client),
                       display_options) {
  if (unsafe_resources.size() == 1) {
    // Log the resource type that triggers the safe browsing blocking page.
    UMA_HISTOGRAM_ENUMERATION(
        "SafeBrowsing.BlockingPage.ResourceType",
        safe_browsing::GetResourceTypeFromRequestDestination(
            unsafe_resources[0].request_destination));
    // Log the request destination that triggers the safe browsing blocking
    // page.
    UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.BlockingPage.RequestDestination",
                              unsafe_resources[0].request_destination);
  }
}

security_interstitials::SecurityInterstitialPage::TypeID
SafeBrowsingBlockingPage::GetTypeForTesting() {
  return SafeBrowsingBlockingPage::kTypeForTesting;
}

}  // namespace weblayer
