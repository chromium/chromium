// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_FACTORY_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}

namespace safe_browsing {
class SafeBrowsingNavigationObserverManager;
}

namespace weblayer {

// Singleton that owns SafeBrowsingNavigationObserverManager objects, one for
// each active BrowserContext. It returns a separate instance if the
// BrowserContext is in incognito mode.
class SafeBrowsingNavigationObserverManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  SafeBrowsingNavigationObserverManagerFactory(
      const SafeBrowsingNavigationObserverManagerFactory&) = delete;
  SafeBrowsingNavigationObserverManagerFactory& operator=(
      const SafeBrowsingNavigationObserverManagerFactory&) = delete;

  // Creates the service if it doesn't exist already for the given
  // |browser_context|. If the service already exists, returns its pointer.
  static safe_browsing::SafeBrowsingNavigationObserverManager*
  GetForBrowserContext(content::BrowserContext* browser_context);

  // Get the singleton instance.
  static SafeBrowsingNavigationObserverManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<SafeBrowsingNavigationObserverManagerFactory>;

  SafeBrowsingNavigationObserverManagerFactory();
  ~SafeBrowsingNavigationObserverManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer
#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_FACTORY_H_
