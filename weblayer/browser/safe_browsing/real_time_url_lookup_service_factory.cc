// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/real_time_url_lookup_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/realtime/url_lookup_service.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/feature_list_creator.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"
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
      nullptr /* identity manager */, nullptr /* profile sync service */,
      static_cast<BrowserContextImpl*>(context)->pref_service(),
      safe_browsing::GetProfileManagementStatus(nullptr),
      false /* is_under_advanced_protection */,
      static_cast<BrowserContextImpl*>(context)->IsOffTheRecord(),
      FeatureListCreator::GetInstance()->variations_service());
}

}  // namespace weblayer
