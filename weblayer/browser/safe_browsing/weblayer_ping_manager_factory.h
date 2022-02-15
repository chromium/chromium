// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_PING_MANAGER_FACTORY_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_PING_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/safe_browsing/core/browser/ping_manager.h"

namespace weblayer {

// Factory for creating the KeyedService PingManager for WebLayer. It returns a
// separate instance for incognito mode.
class WebLayerPingManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static WebLayerPingManagerFactory* GetInstance();
  static safe_browsing::PingManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<WebLayerPingManagerFactory>;

  WebLayerPingManagerFactory();
  ~WebLayerPingManagerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  std::string GetProtocolConfigClientName() const;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_PING_MANAGER_FACTORY_H_
