// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/verdict_cache_manager_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/core/browser/verdict_cache_manager.h"
#include "content/public/browser/browser_context.h"
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
  return new safe_browsing::VerdictCacheManager(
      nullptr /* history service */,
      HostContentSettingsMapFactory::GetForBrowserContext(context));
}

}  // namespace weblayer