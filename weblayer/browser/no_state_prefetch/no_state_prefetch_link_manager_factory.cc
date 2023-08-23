// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/no_state_prefetch/no_state_prefetch_link_manager_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_link_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_manager_factory.h"

namespace weblayer {

// static
prerender::NoStatePrefetchLinkManager*
NoStatePrefetchLinkManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<prerender::NoStatePrefetchLinkManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
NoStatePrefetchLinkManagerFactory*
NoStatePrefetchLinkManagerFactory::GetInstance() {
  return base::Singleton<NoStatePrefetchLinkManagerFactory>::get();
}

NoStatePrefetchLinkManagerFactory::NoStatePrefetchLinkManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "NoStatePrefetchLinkManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(weblayer::NoStatePrefetchManagerFactory::GetInstance());
}

std::unique_ptr<KeyedService>
NoStatePrefetchLinkManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  DCHECK(browser_context);

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(browser_context);
  if (!no_state_prefetch_manager)
    return nullptr;

  return std::make_unique<prerender::NoStatePrefetchLinkManager>(
      no_state_prefetch_manager);
}

content::BrowserContext*
NoStatePrefetchLinkManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* browser_context) const {
  return browser_context;
}

}  // namespace weblayer
