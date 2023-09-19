// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/tracking_protection_settings_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace weblayer {

// static
privacy_sandbox::TrackingProtectionSettings*
TrackingProtectionSettingsFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<privacy_sandbox::TrackingProtectionSettings*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
TrackingProtectionSettingsFactory*
TrackingProtectionSettingsFactory::GetInstance() {
  static base::NoDestructor<TrackingProtectionSettingsFactory> factory;
  return factory.get();
}

TrackingProtectionSettingsFactory::TrackingProtectionSettingsFactory()
    : BrowserContextKeyedServiceFactory(
          "TrackingProtectionSettings",
          BrowserContextDependencyManager::GetInstance()) {}

TrackingProtectionSettingsFactory::~TrackingProtectionSettingsFactory() =
    default;

content::BrowserContext*
TrackingProtectionSettingsFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

std::unique_ptr<KeyedService>
TrackingProtectionSettingsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return std::make_unique<privacy_sandbox::TrackingProtectionSettings>(
      user_prefs::UserPrefs::Get(context), nullptr);
}

}  // namespace weblayer
