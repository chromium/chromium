// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_PING_MANAGER_FACTORY_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_PING_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/safe_browsing/core/browser/ping_manager.h"

namespace weblayer {

// Factory for creating the KeyedService PingManager for WebLayer. It returns
// null for incognito mode.
class WebLayerPingManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static WebLayerPingManagerFactory* GetInstance();
  static safe_browsing::PingManager* GetForBrowserContext(
      content::BrowserContext* context);

  // TODO(crbug.com/1171215): Remove this once browsertests can enable this
  // functionality via the production mechanism for doing so.
  void SignInAccountForTesting();

 private:
  friend class base::NoDestructor<WebLayerPingManagerFactory>;

  WebLayerPingManagerFactory();
  ~WebLayerPingManagerFactory() override;

  // BrowserContextKeyedServiceFactory override:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  bool ShouldFetchAccessTokenForReport(content::BrowserContext* context) const;
  std::string GetProtocolConfigClientName() const;

  // TODO(crbug.com/1171215): Remove this once browsertests can enable this
  // functionality via the production mechanism for doing so.
  bool is_account_signed_in_for_testing_ = false;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_WEBLAYER_PING_MANAGER_FACTORY_H_
