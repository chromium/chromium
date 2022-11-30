// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/no_state_prefetch/no_state_prefetch_manager_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_manager_delegate_impl.h"

namespace weblayer {

// static
prerender::NoStatePrefetchManager*
NoStatePrefetchManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<prerender::NoStatePrefetchManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
NoStatePrefetchManagerFactory* NoStatePrefetchManagerFactory::GetInstance() {
  return base::Singleton<NoStatePrefetchManagerFactory>::get();
}

NoStatePrefetchManagerFactory::NoStatePrefetchManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "NoStatePrefetchManager",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* NoStatePrefetchManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new prerender::NoStatePrefetchManager(
      browser_context,
      std::make_unique<NoStatePrefetchManagerDelegateImpl>(browser_context));
}

content::BrowserContext* NoStatePrefetchManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
