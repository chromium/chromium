// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/stateful_ssl_host_state_delegate_factory.h"

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

namespace weblayer {

// static
StatefulSSLHostStateDelegate*
StatefulSSLHostStateDelegateFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<StatefulSSLHostStateDelegate*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
StatefulSSLHostStateDelegateFactory*
StatefulSSLHostStateDelegateFactory::GetInstance() {
  return base::Singleton<StatefulSSLHostStateDelegateFactory>::get();
}

StatefulSSLHostStateDelegateFactory::StatefulSSLHostStateDelegateFactory()
    : BrowserContextKeyedServiceFactory(
          "StatefulSSLHostStateDelegate",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

StatefulSSLHostStateDelegateFactory::~StatefulSSLHostStateDelegateFactory() =
    default;

KeyedService* StatefulSSLHostStateDelegateFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new StatefulSSLHostStateDelegate(
      context, user_prefs::UserPrefs::Get(context),
      HostContentSettingsMapFactory::GetForBrowserContext(context));
}

content::BrowserContext*
StatefulSSLHostStateDelegateFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
