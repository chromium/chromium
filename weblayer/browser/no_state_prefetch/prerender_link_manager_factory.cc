// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/no_state_prefetch/prerender_link_manager_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/browser/prerender_link_manager.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_manager_factory.h"

namespace weblayer {

// static
prerender::PrerenderLinkManager*
PrerenderLinkManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<prerender::PrerenderLinkManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
PrerenderLinkManagerFactory* PrerenderLinkManagerFactory::GetInstance() {
  return base::Singleton<PrerenderLinkManagerFactory>::get();
}

PrerenderLinkManagerFactory::PrerenderLinkManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PrerenderLinkmanager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(weblayer::NoStatePrefetchManagerFactory::GetInstance());
}

KeyedService* PrerenderLinkManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  DCHECK(browser_context);

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(browser_context);
  if (!no_state_prefetch_manager)
    return nullptr;

  return new prerender::PrerenderLinkManager(no_state_prefetch_manager);
}

content::BrowserContext* PrerenderLinkManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* browser_context) const {
  return browser_context;
}

}  // namespace weblayer