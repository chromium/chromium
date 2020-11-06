// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_LINK_MANAGER_FACTORY_H_
#define WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_LINK_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/no_state_prefetch/browser/prerender_link_manager.h"

namespace content {
class BrowserContext;
}

namespace weblayer {

class PrerenderLinkManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the prerender::PrerenderLinkManager for |context|.
  static prerender::PrerenderLinkManager* GetForBrowserContext(
      content::BrowserContext* context);

  static PrerenderLinkManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PrerenderLinkManagerFactory>;

  PrerenderLinkManagerFactory();
  ~PrerenderLinkManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_LINK_MANAGER_FACTORY_H_
