// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/subresource_filter_client_impl.h"

#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_scope.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"
#include "weblayer/browser/subresource_filter_profile_context_factory.h"

#if defined(OS_ANDROID)
#include "components/safe_browsing/android/remote_database_manager.h"
#include "weblayer/browser/infobar_service.h"
#endif

namespace safe_browsing {
class SafeBrowsingDatabaseManager;
}

namespace weblayer {

namespace {

// Returns a scoped refptr to the SafeBrowsingService's database manager, if
// available. Otherwise returns nullptr.
const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
GetDatabaseManagerFromSafeBrowsingService() {
#if defined(OS_ANDROID)
  SafeBrowsingService* safe_browsing_service =
      BrowserProcess::GetInstance()->GetSafeBrowsingService();
  return safe_browsing_service
             ? scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>(
                   safe_browsing_service->GetSafeBrowsingDBManager())
             : nullptr;
#else
  return nullptr;
#endif
}

}  // namespace

SubresourceFilterClientImpl::SubresourceFilterClientImpl() = default;
SubresourceFilterClientImpl::~SubresourceFilterClientImpl() = default;

// static
void SubresourceFilterClientImpl::CreateThrottleManagerWithClientForWebContents(
    content::WebContents* web_contents) {
  subresource_filter::RulesetService* ruleset_service =
      BrowserProcess::GetInstance()->subresource_filter_ruleset_service();
  subresource_filter::VerifiedRulesetDealer::Handle* dealer =
      ruleset_service ? ruleset_service->GetRulesetDealer() : nullptr;
  subresource_filter::ContentSubresourceFilterThrottleManager::
      CreateForWebContents(
          web_contents, std::make_unique<SubresourceFilterClientImpl>(),
          SubresourceFilterProfileContextFactory::GetForBrowserContext(
              web_contents->GetBrowserContext()),
  // Infobars are supported only on Android in WebLayer. This is not a
  // problem as the subresource filter shows the infobar only on Android
  // as well.
#if defined(OS_ANDROID)
          InfoBarService::FromWebContents(web_contents),
#else
          nullptr,
#endif
          GetDatabaseManagerFromSafeBrowsingService(), dealer);
}

}  // namespace weblayer
