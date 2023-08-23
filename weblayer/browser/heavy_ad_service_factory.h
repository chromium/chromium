// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_HEAVY_AD_SERVICE_FACTORY_H_
#define WEBLAYER_BROWSER_HEAVY_AD_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace heavy_ad_intervention {
class HeavyAdService;
}

namespace weblayer {

class HeavyAdServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  HeavyAdServiceFactory(const HeavyAdServiceFactory&) = delete;
  HeavyAdServiceFactory& operator=(const HeavyAdServiceFactory&) = delete;

  // Gets the HeavyAdService instance for |context|.
  static heavy_ad_intervention::HeavyAdService* GetForBrowserContext(
      content::BrowserContext* context);

  static HeavyAdServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<HeavyAdServiceFactory>;

  HeavyAdServiceFactory();
  ~HeavyAdServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_HEAVY_AD_SERVICE_FACTORY_H_
