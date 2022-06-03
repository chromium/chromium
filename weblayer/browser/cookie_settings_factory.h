// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_COOKIE_SETTINGS_FACTORY_H_
#define WEBLAYER_BROWSER_COOKIE_SETTINGS_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

namespace content_settings {
class CookieSettings;
}

namespace weblayer {

class CookieSettingsFactory
    : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  CookieSettingsFactory(const CookieSettingsFactory&) = delete;
  CookieSettingsFactory& operator=(const CookieSettingsFactory&) = delete;

  static scoped_refptr<content_settings::CookieSettings> GetForBrowserContext(
      content::BrowserContext* browser_context);
  static CookieSettingsFactory* GetInstance();

 private:
  friend class base::NoDestructor<CookieSettingsFactory>;

  CookieSettingsFactory();
  ~CookieSettingsFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_COOKIE_SETTINGS_FACTORY_H_
