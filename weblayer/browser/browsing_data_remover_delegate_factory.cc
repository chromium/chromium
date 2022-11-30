// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/browsing_data_remover_delegate_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "weblayer/browser/browsing_data_remover_delegate.h"

namespace weblayer {

// static
BrowsingDataRemoverDelegate*
BrowsingDataRemoverDelegateFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<BrowsingDataRemoverDelegate*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
BrowsingDataRemoverDelegateFactory*
BrowsingDataRemoverDelegateFactory::GetInstance() {
  static base::NoDestructor<BrowsingDataRemoverDelegateFactory> factory;
  return factory.get();
}

BrowsingDataRemoverDelegateFactory::BrowsingDataRemoverDelegateFactory()
    : BrowserContextKeyedServiceFactory(
          "BrowsingDataRemoverDelegate",
          BrowserContextDependencyManager::GetInstance()) {}

BrowsingDataRemoverDelegateFactory::~BrowsingDataRemoverDelegateFactory() =
    default;

KeyedService* BrowsingDataRemoverDelegateFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BrowsingDataRemoverDelegate(context);
}

content::BrowserContext*
BrowsingDataRemoverDelegateFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
