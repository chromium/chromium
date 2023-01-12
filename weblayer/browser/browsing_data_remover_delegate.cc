// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browsing_data_remover_delegate.h"

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/heavy_ad_intervention/heavy_ad_blocklist.h"
#include "components/heavy_ad_intervention/heavy_ad_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/favicon/favicon_service_impl.h"
#include "weblayer/browser/favicon/favicon_service_impl_factory.h"
#include "weblayer/browser/heavy_ad_service_factory.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"

namespace weblayer {

BrowsingDataRemoverDelegate::BrowsingDataRemoverDelegate(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

BrowsingDataRemoverDelegate::~BrowsingDataRemoverDelegate() = default;

BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher
BrowsingDataRemoverDelegate::GetOriginTypeMatcher() {
  return EmbedderOriginTypeMatcher();
}

bool BrowsingDataRemoverDelegate::MayRemoveDownloadHistory() {
  return true;
}

std::vector<std::string>
BrowsingDataRemoverDelegate::GetDomainsForDeferredCookieDeletion(
    uint64_t remove_mask) {
  return {};
}

void BrowsingDataRemoverDelegate::RemoveEmbedderData(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    uint64_t remove_mask,
    content::BrowsingDataFilterBuilder* filter_builder,
    uint64_t origin_type_mask,
    base::OnceCallback<void(uint64_t)> callback) {
  callback_ = std::move(callback);

  // Note: if history is ever added to WebLayer, also remove isolated origins
  // when history is cleared.
  if (remove_mask & DATA_TYPE_ISOLATED_ORIGINS) {
    browsing_data::RemoveSiteIsolationData(
        user_prefs::UserPrefs::Get(browser_context_));
  }

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForBrowserContext(browser_context_);

  if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_CACHE) {
    browsing_data::RemovePrerenderCacheData(
        NoStatePrefetchManagerFactory::GetForBrowserContext(browser_context_));
  }

  if (remove_mask & DATA_TYPE_FAVICONS) {
    auto* service =
        FaviconServiceImplFactory::GetForBrowserContext(browser_context_);
    if (service) {
      // The favicon database doesn't track enough information to remove
      // favicons in a time range. Delete everything.
      service->DeleteAndRecreateDatabase(CreateTaskCompletionClosure());
    }
  }

  if (remove_mask & DATA_TYPE_AD_INTERVENTIONS) {
    heavy_ad_intervention::HeavyAdService* heavy_ad_service =
        HeavyAdServiceFactory::GetForBrowserContext(browser_context_);
    if (heavy_ad_service->heavy_ad_blocklist()) {
      heavy_ad_service->heavy_ad_blocklist()->ClearBlockList(delete_begin,
                                                             delete_end);
    }
  }

  // We ignore the DATA_TYPE_COOKIES request if UNPROTECTED_WEB is not set,
  // so that callers who request COOKIES_AND_SITE_DATA with PROTECTED_WEB
  // don't accidentally remove the cookies that are associated with the
  // UNPROTECTED_WEB origin. This is necessary because cookies are not separated
  // between UNPROTECTED_WEB and PROTECTED_WEB.
  if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_COOKIES) {
    network::mojom::NetworkContext* safe_browsing_context = nullptr;
#if BUILDFLAG(IS_ANDROID)
    safe_browsing_context = BrowserProcess::GetInstance()
                                ->GetSafeBrowsingService()
                                ->GetNetworkContext();
#endif
    browsing_data::RemoveEmbedderCookieData(
        delete_begin, delete_end, filter_builder, host_content_settings_map,
        safe_browsing_context,
        base::BindOnce(
            &BrowsingDataRemoverDelegate::CreateTaskCompletionClosure,
            base::Unretained(this)));
  }

  if (remove_mask & DATA_TYPE_SITE_SETTINGS) {
    browsing_data::RemoveSiteSettingsData(delete_begin, delete_end,
                                          host_content_settings_map);
    content::OriginTrialsControllerDelegate* delegate =
        browser_context_->GetOriginTrialsControllerDelegate();
    if (delegate)
      delegate->ClearPersistedTokens();
  }

  RunCallbackIfDone();
}

base::OnceClosure BrowsingDataRemoverDelegate::CreateTaskCompletionClosure() {
  ++pending_tasks_;

  return base::BindOnce(&BrowsingDataRemoverDelegate::OnTaskComplete,
                        weak_ptr_factory_.GetWeakPtr());
}

void BrowsingDataRemoverDelegate::OnTaskComplete() {
  pending_tasks_--;
  RunCallbackIfDone();
}

void BrowsingDataRemoverDelegate::RunCallbackIfDone() {
  if (pending_tasks_ != 0)
    return;

  std::move(callback_).Run(/*failed_data_types=*/0);
}

}  // namespace weblayer
