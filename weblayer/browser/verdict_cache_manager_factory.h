// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_VERDICT_CACHE_MANAGER_FACTORY_H_
#define WEBLAYER_BROWSER_VERDICT_CACHE_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}

namespace safe_browsing {
class VerdictCacheManager;
}

namespace weblayer {

// Singleton that owns VerdictCacheManager objects and associates them
// them with BrowserContextImpl instances.
class VerdictCacheManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Creates the manager if it doesn't exist already for the given
  // |browser_context|. If the manager already exists, return its pointer.
  static safe_browsing::VerdictCacheManager* GetForBrowserContext(
      content::BrowserContext* browser_context);

  // Get the singleton instance.
  static VerdictCacheManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<VerdictCacheManagerFactory>;

  VerdictCacheManagerFactory();
  ~VerdictCacheManagerFactory() override = default;
  VerdictCacheManagerFactory(const VerdictCacheManagerFactory&) = delete;
  VerdictCacheManagerFactory& operator=(const VerdictCacheManagerFactory&) =
      delete;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_VERDICT_CACHE_MANAGER_FACTORY_H_