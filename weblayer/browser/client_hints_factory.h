// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_CLIENT_HINTS_FACTORY_H_
#define WEBLAYER_BROWSER_CLIENT_HINTS_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace client_hints {
class ClientHints;
}

namespace weblayer {

class ClientHintsFactory : public BrowserContextKeyedServiceFactory {
 public:
  ClientHintsFactory(const ClientHintsFactory&) = delete;
  ClientHintsFactory& operator=(const ClientHintsFactory&) = delete;

  static client_hints::ClientHints* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static ClientHintsFactory* GetInstance();

 private:
  friend class base::NoDestructor<ClientHintsFactory>;

  ClientHintsFactory();
  ~ClientHintsFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_CLIENT_HINTS_FACTORY_H_
