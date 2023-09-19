// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_TRACKING_PROTECTION_SETTINGS_FACTORY_H_
#define WEBLAYER_BROWSER_TRACKING_PROTECTION_SETTINGS_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace privacy_sandbox {
class TrackingProtectionSettings;
}

namespace weblayer {

class TrackingProtectionSettingsFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  TrackingProtectionSettingsFactory(const TrackingProtectionSettingsFactory&) =
      delete;
  TrackingProtectionSettingsFactory& operator=(
      const TrackingProtectionSettingsFactory&) = delete;

  static scoped_refptr<privacy_sandbox::TrackingProtectionSettings>
  GetForBrowserContext(content::BrowserContext* browser_context);
  static TrackingProtectionSettingsFactory* GetInstance();

 private:
  friend class base::NoDestructor<TrackingProtectionSettingsFactory>;

  TrackingProtectionSettingsFactory();
  ~TrackingProtectionSettingsFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_TRACKING_PROTECTION_SETTINGS_FACTORY_H_
