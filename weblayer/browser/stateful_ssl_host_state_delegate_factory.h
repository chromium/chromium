// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_STATEFUL_SSL_HOST_STATE_DELEGATE_FACTORY_H_
#define WEBLAYER_BROWSER_STATEFUL_SSL_HOST_STATE_DELEGATE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/prefs/pref_service.h"

class StatefulSSLHostStateDelegate;

namespace weblayer {

// Singleton that associates all StatefulSSLHostStateDelegates with
// BrowserContexts.
class StatefulSSLHostStateDelegateFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static StatefulSSLHostStateDelegate* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static StatefulSSLHostStateDelegateFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      StatefulSSLHostStateDelegateFactory>;

  StatefulSSLHostStateDelegateFactory();
  ~StatefulSSLHostStateDelegateFactory() override;
  StatefulSSLHostStateDelegateFactory(
      const StatefulSSLHostStateDelegateFactory&) = delete;
  StatefulSSLHostStateDelegateFactory& operator=(
      const StatefulSSLHostStateDelegateFactory&) = delete;

  // BrowserContextKeyedServiceFactory methods:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_STATEFUL_SSL_HOST_STATE_DELEGATE_FACTORY_H_
