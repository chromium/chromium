// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_blocking_page.h"

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "content/public/browser/navigation_entry.h"
#include "weblayer/browser/browser_context_impl.h"
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
                       display_options) {}

SafeBrowsingBlockingPage* SafeBrowsingBlockingPage::CreateBlockingPage(
    SafeBrowsingUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResource& unsafe_resource) {
  const UnsafeResourceList unsafe_resources{unsafe_resource};
  content::NavigationEntry* entry =
      security_interstitials::GetNavigationEntryForResource(unsafe_resource);
  GURL url =
      (main_frame_url.is_empty() && entry) ? entry->GetURL() : main_frame_url;

  BrowserContextImpl* browser_context =
      static_cast<BrowserContextImpl*>(web_contents->GetBrowserContext());
  security_interstitials::BaseSafeBrowsingErrorUI::SBErrorDisplayOptions
      display_options =
          BaseBlockingPage::CreateDefaultDisplayOptions(unsafe_resources);
  display_options.is_extended_reporting_opt_in_allowed =
      safe_browsing::IsExtendedReportingOptInAllowed(
          *(browser_context->pref_service()));
  display_options.is_extended_reporting_enabled =
      safe_browsing::IsExtendedReportingEnabled(
          *(browser_context->pref_service()));

  // TODO(crbug.com/1080748): Set settings_page_helper once enhanced protection
  // is supported on weblayer.
  return new SafeBrowsingBlockingPage(
      ui_manager, web_contents, url, unsafe_resources,
      CreateControllerClient(web_contents, unsafe_resources, ui_manager,
                             browser_context->pref_service(),
                             /*settings_page_helper*/ nullptr),
      display_options);
}

security_interstitials::SecurityInterstitialPage::TypeID
SafeBrowsingBlockingPage::GetTypeForTesting() {
  return SafeBrowsingBlockingPage::kTypeForTesting;
}

}  // namespace weblayer
