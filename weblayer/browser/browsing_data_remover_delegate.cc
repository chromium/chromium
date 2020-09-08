// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browsing_data_remover_delegate.h"

#include "base/callback.h"
#include "build/build_config.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/default_search_engine.h"
#include "weblayer/browser/favicon/favicon_service_impl.h"
#include "weblayer/browser/favicon/favicon_service_impl_factory.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/no_state_prefetch/prerender_manager_factory.h"
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
        PrerenderManagerFactory::GetForBrowserContext(browser_context_));
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

  // We ignore the DATA_TYPE_COOKIES request if UNPROTECTED_WEB is not set,
  // so that callers who request COOKIES_AND_SITE_DATA with PROTECTED_WEB
  // don't accidentally remove the cookies that are associated with the
  // UNPROTECTED_WEB origin. This is necessary because cookies are not separated
  // between UNPROTECTED_WEB and PROTECTED_WEB.
  if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_COOKIES) {
    network::mojom::NetworkContext* safe_browsing_context = nullptr;
#if defined(OS_ANDROID)
    auto* sb_service = BrowserProcess::GetInstance()->GetSafeBrowsingService();
    if (sb_service)
      safe_browsing_context = sb_service->GetNetworkContext();
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

    // Reset the Default Search Engine permissions to their default.
    ResetDsePermissions(browser_context_);
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
