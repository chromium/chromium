// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/heavy_ad_service_factory.h"

#include "base/no_destructor.h"
#include "components/heavy_ad_intervention/heavy_ad_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace weblayer {

// static
heavy_ad_intervention::HeavyAdService*
HeavyAdServiceFactory::GetForBrowserContext(content::BrowserContext* context) {
  return static_cast<heavy_ad_intervention::HeavyAdService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
HeavyAdServiceFactory* HeavyAdServiceFactory::GetInstance() {
  static base::NoDestructor<HeavyAdServiceFactory> factory;
  return factory.get();
}

HeavyAdServiceFactory::HeavyAdServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "HeavyAdService",
          BrowserContextDependencyManager::GetInstance()) {}

HeavyAdServiceFactory::~HeavyAdServiceFactory() = default;

KeyedService* HeavyAdServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new heavy_ad_intervention::HeavyAdService();
}

content::BrowserContext* HeavyAdServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
