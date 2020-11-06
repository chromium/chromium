// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/no_state_prefetch/prerender_link_manager_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/no_state_prefetch/browser/prerender_link_manager.h"
#include "components/no_state_prefetch/browser/prerender_manager.h"
#include "weblayer/browser/no_state_prefetch/prerender_manager_factory.h"

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
  DependsOn(weblayer::PrerenderManagerFactory::GetInstance());
}

KeyedService* PrerenderLinkManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  DCHECK(browser_context);

  prerender::PrerenderManager* prerender_manager =
      PrerenderManagerFactory::GetForBrowserContext(browser_context);
  if (!prerender_manager)
    return nullptr;

  return new prerender::PrerenderLinkManager(prerender_manager);
}

content::BrowserContext* PrerenderLinkManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* browser_context) const {
  return browser_context;
}

}  // namespace weblayer