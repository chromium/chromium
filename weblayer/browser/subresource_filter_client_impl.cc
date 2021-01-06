// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/subresource_filter_client_impl.h"

#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/profile_interaction_manager.h"
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
#include "components/subresource_filter/android/ads_blocked_infobar_delegate.h"
#include "weblayer/browser/infobar_service.h"
#endif

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

SubresourceFilterClientImpl::SubresourceFilterClientImpl(
    content::WebContents* web_contents)
    :
#if defined(OS_ANDROID)
      web_contents_(web_contents),
#endif
      database_manager_(GetDatabaseManagerFromSafeBrowsingService()),
      profile_interaction_manager_(
          std::make_unique<subresource_filter::ProfileInteractionManager>(
              web_contents,
              SubresourceFilterProfileContextFactory::GetForBrowserContext(
                  web_contents->GetBrowserContext()))) {
}

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
          web_contents,
          std::make_unique<SubresourceFilterClientImpl>(web_contents), dealer);
}

void SubresourceFilterClientImpl::OnReloadRequested() {
  // TODO(crbug.com/1116095): Bring up this flow on Android when user requests
  // it via the infobar.
  NOTIMPLEMENTED();
}

void SubresourceFilterClientImpl::ShowNotification() {
#if defined(OS_ANDROID)
  // TODO(crbug.com/1116095): Move ChromeSubresourceFilterClient::ShowUI()'s
  // interaction with metrics and content settings into code that's shared by
  // WebLayer.
  subresource_filter::AdsBlockedInfobarDelegate::Create(
      InfoBarService::FromWebContents(web_contents_));
#endif
}

void SubresourceFilterClientImpl::OnAdsViolationTriggered(
    content::RenderFrameHost* rfh,
    subresource_filter::mojom::AdsViolation triggered_violation) {}

const scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
SubresourceFilterClientImpl::GetSafeBrowsingDatabaseManager() {
  return database_manager_;
}

subresource_filter::ProfileInteractionManager*
SubresourceFilterClientImpl::GetProfileInteractionManager() {
  return profile_interaction_manager_.get();
}

}  // namespace weblayer
