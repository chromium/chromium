// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_MANAGER_FACTORY_H_
#define WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace prerender {
class NoStatePrefetchManager;
}

namespace weblayer {

// Singleton that owns all NoStatePrefetchManagers and associates them with
// BrowserContexts. Listens for the BrowserContext's destruction notification
// and cleans up the associated NoStatePrefetchManager.
class NoStatePrefetchManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the NoStatePrefetchManager for |context|.
  static prerender::NoStatePrefetchManager* GetForBrowserContext(
      content::BrowserContext* context);

  static NoStatePrefetchManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<NoStatePrefetchManagerFactory>;

  NoStatePrefetchManagerFactory();
  ~NoStatePrefetchManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NO_STATE_PREFETCH_NO_STATE_PREFETCH_MANAGER_FACTORY_H_
