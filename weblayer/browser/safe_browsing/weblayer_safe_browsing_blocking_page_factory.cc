// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/weblayer_safe_browsing_blocking_page_factory.h"

#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "content/public/browser/navigation_entry.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "weblayer/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"

namespace weblayer {

safe_browsing::SafeBrowsingBlockingPage*
WebLayerSafeBrowsingBlockingPageFactory::CreateSafeBrowsingPage(
    safe_browsing::BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList&
        unsafe_resources,
    bool should_trigger_reporting) {
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
      ui_manager, web_contents, main_frame_url, unsafe_resources,
      safe_browsing::BaseBlockingPage::CreateControllerClient(
          web_contents, unsafe_resources, ui_manager,
          browser_context->pref_service(),
          /*settings_page_helper*/ nullptr),
      display_options, should_trigger_reporting,
      // WebLayer doesn't integrate //components/history.
      /*history_service=*/nullptr,
      SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
          browser_context),
      SafeBrowsingMetricsCollectorFactory::GetForBrowserContext(
          browser_context),
      BrowserProcess::GetInstance()
          ->GetSafeBrowsingService()
          ->GetTriggerManager(),
      safe_browsing::IsSafeBrowsingProceedAnywayDisabled(
          *(browser_context->pref_service())),
      // HaTS surveys are not supported for Weblayer.
      /*is_safe_browsing_surveys_enabled=*/false,
      /*url_loader_for_testing=*/nullptr);
}

}  // namespace weblayer
