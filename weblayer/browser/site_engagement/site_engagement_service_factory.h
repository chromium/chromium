// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SITE_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_FACTORY_H_
#define WEBLAYER_BROWSER_SITE_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/site_engagement/content/site_engagement_service.h"

namespace weblayer {

// Singleton that owns all SiteEngagementServices and associates them with
// BrowserContexts.
class SiteEngagementServiceFactory
    : public BrowserContextKeyedServiceFactory,
      public site_engagement::SiteEngagementService::ServiceProvider {
 public:
  SiteEngagementServiceFactory(const SiteEngagementServiceFactory&) = delete;
  SiteEngagementServiceFactory& operator=(const SiteEngagementServiceFactory&) =
      delete;

  // Creates the service if it doesn't already exist for the given
  // |browser_context|. If the service already exists, return it.
  static site_engagement::SiteEngagementService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static SiteEngagementServiceFactory* GetInstance();

  // SiteEngagementService::ServiceProvider:
  site_engagement::SiteEngagementService* GetSiteEngagementService(
      content::BrowserContext* browser_context) override;

 private:
  friend class base::NoDestructor<SiteEngagementServiceFactory>;

  SiteEngagementServiceFactory();
  ~SiteEngagementServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SITE_ENGAGEMENT_SITE_ENGAGEMENT_SERVICE_FACTORY_H_
