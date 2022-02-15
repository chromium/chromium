// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/weblayer_ping_manager_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/profile_impl.h"

namespace weblayer {

// static
WebLayerPingManagerFactory* WebLayerPingManagerFactory::GetInstance() {
  static base::NoDestructor<WebLayerPingManagerFactory> instance;
  return instance.get();
}

// static
safe_browsing::PingManager* WebLayerPingManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<safe_browsing::PingManager*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

WebLayerPingManagerFactory::WebLayerPingManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "WeblayerSafeBrowsingPingManager",
          BrowserContextDependencyManager::GetInstance()) {}

WebLayerPingManagerFactory::~WebLayerPingManagerFactory() = default;

KeyedService* WebLayerPingManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return safe_browsing::PingManager::Create(safe_browsing::GetV4ProtocolConfig(
      GetProtocolConfigClientName(), /*disable_auto_update=*/false));
}

content::BrowserContext* WebLayerPingManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

std::string WebLayerPingManagerFactory::GetProtocolConfigClientName() const {
  // Return a weblayer specific client name.
  return "weblayer";
}

}  // namespace weblayer
