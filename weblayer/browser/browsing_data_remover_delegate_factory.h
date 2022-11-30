// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BROWSING_DATA_REMOVER_DELEGATE_FACTORY_H_
#define WEBLAYER_BROWSER_BROWSING_DATA_REMOVER_DELEGATE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace weblayer {
class BrowsingDataRemoverDelegate;

class BrowsingDataRemoverDelegateFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  BrowsingDataRemoverDelegateFactory(
      const BrowsingDataRemoverDelegateFactory&) = delete;
  BrowsingDataRemoverDelegateFactory& operator=(
      const BrowsingDataRemoverDelegateFactory&) = delete;

  static BrowsingDataRemoverDelegate* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static BrowsingDataRemoverDelegateFactory* GetInstance();

 private:
  friend class base::NoDestructor<BrowsingDataRemoverDelegateFactory>;

  BrowsingDataRemoverDelegateFactory();
  ~BrowsingDataRemoverDelegateFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BROWSING_DATA_REMOVER_DELEGATE_FACTORY_H_
