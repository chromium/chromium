// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/real_time_url_lookup_service_factory.h"

#include "base/bind.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/feature_list_creator.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"
#include "weblayer/browser/safe_browsing/safe_browsing_token_fetcher_impl.h"
#include "weblayer/browser/safe_browsing/weblayer_user_population_helper.h"
#include "weblayer/browser/verdict_cache_manager_factory.h"

namespace weblayer {

// static
safe_browsing::RealTimeUrlLookupService*
RealTimeUrlLookupServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<safe_browsing::RealTimeUrlLookupService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /* create= */ true));
}

// static
RealTimeUrlLookupServiceFactory*
RealTimeUrlLookupServiceFactory::GetInstance() {
  return base::Singleton<RealTimeUrlLookupServiceFactory>::get();
}

RealTimeUrlLookupServiceFactory::RealTimeUrlLookupServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "RealTimeUrlLookupService",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* RealTimeUrlLookupServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto url_loader_factory =
      std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
          BrowserProcess::GetInstance()
              ->GetSafeBrowsingService()
              ->GetURLLoaderFactory());

  return new safe_browsing::RealTimeUrlLookupService(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)),
      VerdictCacheManagerFactory::GetForBrowserContext(context),
      base::BindRepeating(&GetUserPopulationForBrowserContext, context),
      static_cast<BrowserContextImpl*>(context)->pref_service(),
      std::make_unique<SafeBrowsingTokenFetcherImpl>(base::BindRepeating(
          &ProfileImpl::access_token_fetch_delegate,
          base::Unretained(ProfileImpl::FromBrowserContext(context)))),
      // TODO(crbug.com/1171215): Change this to production mechanism for
      // enabling Gaia-keyed URL lookups once that mechanism is determined.
      base::BindRepeating(&RealTimeUrlLookupServiceFactory::
                              access_token_fetches_enabled_for_testing,
                          base::Unretained(this)),
      static_cast<BrowserContextImpl*>(context)->IsOffTheRecord(),
      FeatureListCreator::GetInstance()->variations_service(),
      // Referrer chain provider is currently not available on WebLayer. Once it
      // is implemented, inject it to enable referrer chain in real time
      // requests.
      /*referrer_chain_provider=*/nullptr);
}

}  // namespace weblayer
