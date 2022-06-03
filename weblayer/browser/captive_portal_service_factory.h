// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_CAPTIVE_PORTAL_SERVICE_FACTORY_H_
#define WEBLAYER_BROWSER_CAPTIVE_PORTAL_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace captive_portal {
class CaptivePortalService;
}

namespace weblayer {

// Singleton that owns all captive_portal::CaptivePortalServices and associates
// them with BrowserContextImpl instances.
class CaptivePortalServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the captive_portal::CaptivePortalService for |browser_context|.
  static captive_portal::CaptivePortalService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static CaptivePortalServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<CaptivePortalServiceFactory>;

  CaptivePortalServiceFactory();
  ~CaptivePortalServiceFactory() override;
  CaptivePortalServiceFactory(const CaptivePortalServiceFactory&) = delete;
  CaptivePortalServiceFactory& operator=(const CaptivePortalServiceFactory&) =
      delete;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  // Incognito profiles have their own instance of
  // captive_portal::CaptivePortalService rather than the default behavior of
  // the service being null if the profile is incognito.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  //  namespace weblayer

#endif  // WEBLAYER_BROWSER_CAPTIVE_PORTAL_SERVICE_FACTORY_H_
