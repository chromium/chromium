// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/no_state_prefetch/prerender_manager_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/no_state_prefetch/browser/prerender_manager.h"
#include "weblayer/browser/no_state_prefetch/prerender_manager_delegate_impl.h"

namespace weblayer {

// static
prerender::PrerenderManager* PrerenderManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<prerender::PrerenderManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PrerenderManagerFactory* PrerenderManagerFactory::GetInstance() {
  return base::Singleton<PrerenderManagerFactory>::get();
}

PrerenderManagerFactory::PrerenderManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PrerenderManager",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService* PrerenderManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new prerender::PrerenderManager(
      browser_context,
      std::make_unique<PrerenderManagerDelegateImpl>(browser_context));
}

content::BrowserContext* PrerenderManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
