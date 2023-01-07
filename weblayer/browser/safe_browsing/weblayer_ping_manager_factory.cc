// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/weblayer_ping_manager_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"
#include "weblayer/browser/safe_browsing/safe_browsing_token_fetcher_impl.h"
#include "weblayer/browser/safe_browsing/weblayer_user_population_helper.h"

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
  return safe_browsing::PingManager::Create(
      safe_browsing::GetV4ProtocolConfig(GetProtocolConfigClientName(),
                                         /*disable_auto_update=*/false),
      // TODO(crbug.com/1233532): Should WebLayer support the
      // kSafeBrowsingSeparateNetworkContexts feature?
      BrowserProcess::GetInstance()
          ->GetSafeBrowsingService()
          ->GetURLLoaderFactory(),
      std::make_unique<SafeBrowsingTokenFetcherImpl>(base::BindRepeating(
          &ProfileImpl::access_token_fetch_delegate,
          base::Unretained(ProfileImpl::FromBrowserContext(context)))),
      base::BindRepeating(
          &WebLayerPingManagerFactory::ShouldFetchAccessTokenForReport,
          base::Unretained(this), context),
      safe_browsing::WebUIInfoSingleton::GetInstance(),
      content::GetUIThreadTaskRunner({}),
      base::BindRepeating(&GetUserPopulationForBrowserContext, context));
}

bool WebLayerPingManagerFactory::ShouldFetchAccessTokenForReport(
    content::BrowserContext* context) const {
  PrefService* pref_service =
      static_cast<BrowserContextImpl*>(context)->pref_service();
  return base::FeatureList::IsEnabled(
             safe_browsing::kSafeBrowsingCsbrrWithToken) &&
         safe_browsing::IsEnhancedProtectionEnabled(*pref_service) &&
         // TODO(crbug.com/1171215): Change this to production mechanism for
         // enabling Gaia-keyed client reports once that mechanism is
         // determined.
         is_account_signed_in_for_testing_;
}

std::string WebLayerPingManagerFactory::GetProtocolConfigClientName() const {
  // Return a weblayer specific client name.
  return "weblayer";
}

// TODO(crbug.com/1171215): Remove this once browsertests can enable this
// functionality via the production mechanism for doing so.
void WebLayerPingManagerFactory::SignInAccountForTesting() {
  is_account_signed_in_for_testing_ = true;
}

}  // namespace weblayer
