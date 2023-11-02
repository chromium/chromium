// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/verdict_cache_manager_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "content/public/browser/browser_context.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

namespace weblayer {

// static
safe_browsing::VerdictCacheManager*
VerdictCacheManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<safe_browsing::VerdictCacheManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /* create= */ true));
}

// static
VerdictCacheManagerFactory* VerdictCacheManagerFactory::GetInstance() {
  return base::Singleton<VerdictCacheManagerFactory>::get();
}

VerdictCacheManagerFactory::VerdictCacheManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "VerdictCacheManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

KeyedService* VerdictCacheManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  BrowserContextImpl* context_impl = static_cast<BrowserContextImpl*>(context);
  return new safe_browsing::VerdictCacheManager(
      /*history_service=*/nullptr,
      HostContentSettingsMapFactory::GetForBrowserContext(context),
      context_impl->pref_service(), /*sync_observer=*/nullptr);
}

}  // namespace weblayer
