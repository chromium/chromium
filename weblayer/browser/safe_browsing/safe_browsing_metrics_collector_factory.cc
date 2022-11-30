// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "weblayer/browser/browser_context_impl.h"

namespace weblayer {

// static
safe_browsing::SafeBrowsingMetricsCollector*
SafeBrowsingMetricsCollectorFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<safe_browsing::SafeBrowsingMetricsCollector*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /* create= */ true));
}

// static
SafeBrowsingMetricsCollectorFactory*
SafeBrowsingMetricsCollectorFactory::GetInstance() {
  static base::NoDestructor<SafeBrowsingMetricsCollectorFactory> factory;
  return factory.get();
}

// static
SafeBrowsingMetricsCollectorFactory::SafeBrowsingMetricsCollectorFactory()
    : BrowserContextKeyedServiceFactory(
          "SafeBrowsingMetricsCollector",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* SafeBrowsingMetricsCollectorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  BrowserContextImpl* context_impl = static_cast<BrowserContextImpl*>(context);
  return new safe_browsing::SafeBrowsingMetricsCollector(
      context_impl->pref_service());
}

}  // namespace weblayer
