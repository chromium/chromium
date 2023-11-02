// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "weblayer/browser/browser_context_impl.h"

namespace weblayer {

// static
safe_browsing::SafeBrowsingNavigationObserverManager*
SafeBrowsingNavigationObserverManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<safe_browsing::SafeBrowsingNavigationObserverManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/true));
}

// static
SafeBrowsingNavigationObserverManagerFactory*
SafeBrowsingNavigationObserverManagerFactory::GetInstance() {
  static base::NoDestructor<SafeBrowsingNavigationObserverManagerFactory>
      factory;
  return factory.get();
}

SafeBrowsingNavigationObserverManagerFactory::
    SafeBrowsingNavigationObserverManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "SafeBrowsingNavigationObserverManager",
          BrowserContextDependencyManager::GetInstance()) {}

KeyedService*
SafeBrowsingNavigationObserverManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  BrowserContextImpl* context_impl = static_cast<BrowserContextImpl*>(context);
  return new safe_browsing::SafeBrowsingNavigationObserverManager(
      context_impl->pref_service());
}

content::BrowserContext*
SafeBrowsingNavigationObserverManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
