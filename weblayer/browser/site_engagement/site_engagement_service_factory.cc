// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/site_engagement/site_engagement_service_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

using site_engagement::SiteEngagementService;

namespace weblayer {

// static
SiteEngagementService* SiteEngagementServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<SiteEngagementService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/true));
}

// static
SiteEngagementServiceFactory* SiteEngagementServiceFactory::GetInstance() {
  static base::NoDestructor<SiteEngagementServiceFactory> factory;
  return factory.get();
}

SiteEngagementServiceFactory::SiteEngagementServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SiteEngagementService",
          BrowserContextDependencyManager::GetInstance()) {
  SiteEngagementService::SetServiceProvider(this);
}

SiteEngagementServiceFactory::~SiteEngagementServiceFactory() {
  SiteEngagementService::ClearServiceProvider(this);
}

KeyedService* SiteEngagementServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new SiteEngagementService(browser_context);
}

content::BrowserContext* SiteEngagementServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

SiteEngagementService* SiteEngagementServiceFactory::GetSiteEngagementService(
    content::BrowserContext* browser_context) {
  return GetForBrowserContext(browser_context);
}

}  // namespace weblayer
