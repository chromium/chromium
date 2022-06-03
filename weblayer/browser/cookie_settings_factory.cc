// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/cookie_settings_factory.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

namespace weblayer {

// static
scoped_refptr<content_settings::CookieSettings>
CookieSettingsFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<content_settings::CookieSettings*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true).get());
}

// static
CookieSettingsFactory* CookieSettingsFactory::GetInstance() {
  static base::NoDestructor<CookieSettingsFactory> factory;
  return factory.get();
}

CookieSettingsFactory::CookieSettingsFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
          "CookieSettings",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

CookieSettingsFactory::~CookieSettingsFactory() = default;

void CookieSettingsFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  content_settings::CookieSettings::RegisterProfilePrefs(registry);
}

content::BrowserContext* CookieSettingsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

scoped_refptr<RefcountedKeyedService>
CookieSettingsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new content_settings::CookieSettings(
      HostContentSettingsMapFactory::GetForBrowserContext(context),
      static_cast<BrowserContextImpl*>(context)->pref_service(),
      context->IsOffTheRecord());
}

}  // namespace weblayer
