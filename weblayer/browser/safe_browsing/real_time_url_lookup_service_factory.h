// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_REAL_TIME_URL_LOOKUP_SERVICE_FACTORY_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_REAL_TIME_URL_LOOKUP_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}

namespace safe_browsing {
class RealTimeUrlLookupService;
}  // namespace safe_browsing

namespace weblayer {

// Singleton that owns RealTimeUrlLookupService objects and associates them
// them with BrowserContextImpl instances.
class RealTimeUrlLookupServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Creates the service if it doesn't exist already for the given
  // |browser_context|. If the service already exists, return its pointer.
  static safe_browsing::RealTimeUrlLookupService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  // Get the singleton instance.
  static RealTimeUrlLookupServiceFactory* GetInstance();

  // TODO(crbug.com/1171215): Remove this once browsertests can enable this
  // functionality via the production mechanism for doing so.
  void set_access_token_fetches_enabled_for_testing() {
    access_token_fetches_enabled_for_testing_ = true;
  }

 private:
  friend struct base::DefaultSingletonTraits<RealTimeUrlLookupServiceFactory>;

  RealTimeUrlLookupServiceFactory();
  ~RealTimeUrlLookupServiceFactory() override = default;
  RealTimeUrlLookupServiceFactory(const RealTimeUrlLookupServiceFactory&) =
      delete;
  RealTimeUrlLookupServiceFactory& operator=(
      const RealTimeUrlLookupServiceFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  // TODO(crbug.com/1171215): Remove this once browsertests can enable this
  // functionality via the production mechanism for doing so.
  bool access_token_fetches_enabled_for_testing(
      bool user_has_enabled_enhanced_protection) const {
    return access_token_fetches_enabled_for_testing_;
  }

  // TODO(crbug.com/1171215): Remove this once browsertests can enable this
  // functionality via the production mechanism for doing so.
  bool access_token_fetches_enabled_for_testing_ = false;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_REAL_TIME_URL_LOOKUP_SERVICE_FACTORY_H_
