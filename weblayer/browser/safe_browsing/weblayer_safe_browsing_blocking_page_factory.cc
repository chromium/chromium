// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/weblayer_safe_browsing_blocking_page_factory.h"

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "content/public/browser/navigation_entry.h"
#include "weblayer/browser/browser_context_impl.h"

namespace weblayer {

safe_browsing::SafeBrowsingBlockingPage*
WebLayerSafeBrowsingBlockingPageFactory::CreateSafeBrowsingPage(
    safe_browsing::BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList&
        unsafe_resources,
    bool should_trigger_reporting) {
  // TODO(crbug.com/1231997): Is this adjusting of main_frame_url necessary?
  // //chrome doesn't seem to do it.
  content::NavigationEntry* entry =
      security_interstitials::GetNavigationEntryForResource(
          unsafe_resources[0]);
  GURL url =
      (main_frame_url.is_empty() && entry) ? entry->GetURL() : main_frame_url;

  BrowserContextImpl* browser_context =
      static_cast<BrowserContextImpl*>(web_contents->GetBrowserContext());
  security_interstitials::BaseSafeBrowsingErrorUI::SBErrorDisplayOptions
      display_options =
          safe_browsing::BaseBlockingPage::CreateDefaultDisplayOptions(
              unsafe_resources);
  display_options.is_extended_reporting_opt_in_allowed =
      safe_browsing::IsExtendedReportingOptInAllowed(
          *(browser_context->pref_service()));
  display_options.is_extended_reporting_enabled =
      safe_browsing::IsExtendedReportingEnabled(
          *(browser_context->pref_service()));

  // TODO(crbug.com/1080748): Set settings_page_helper once enhanced protection
  // is supported on weblayer.
  return new safe_browsing::SafeBrowsingBlockingPage(
      ui_manager, web_contents, url, unsafe_resources,
      safe_browsing::BaseBlockingPage::CreateControllerClient(
          web_contents, unsafe_resources, ui_manager,
          browser_context->pref_service(),
          /*settings_page_helper*/ nullptr),
      display_options, should_trigger_reporting,
      // WebLayer doesn't integrate //components/history.
      /*history_service=*/nullptr,
      // TODO(crbug.com/1231997): Consider bringing up each of the following
      // three objects in WL.
      /*safe_browsing_navigation_observer_manager=*/nullptr,
      /*safe_browsing_metrics_collector=*/nullptr,
      /*trigger_manager=*/nullptr);
}

}  // namespace weblayer
