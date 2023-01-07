// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}

namespace safe_browsing {
class SafeBrowsingMetricsCollector;
}

namespace weblayer {

// Singleton that owns SafeBrowsingMetricsCollector objects, one for each active
// BrowserContext. It returns nullptr in incognito mode.
class SafeBrowsingMetricsCollectorFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  SafeBrowsingMetricsCollectorFactory(
      const SafeBrowsingMetricsCollectorFactory&) = delete;
  SafeBrowsingMetricsCollectorFactory& operator=(
      const SafeBrowsingMetricsCollectorFactory&) = delete;

  // Creates the object if it doesn't exist already for the given
  // |browser_context|. If the object already exists, returns its pointer.
  static safe_browsing::SafeBrowsingMetricsCollector* GetForBrowserContext(
      content::BrowserContext* browser_context);

  // Get the singleton instance.
  static SafeBrowsingMetricsCollectorFactory* GetInstance();

 private:
  friend class base::NoDestructor<SafeBrowsingMetricsCollectorFactory>;

  SafeBrowsingMetricsCollectorFactory();
  ~SafeBrowsingMetricsCollectorFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_METRICS_COLLECTOR_FACTORY_H_
