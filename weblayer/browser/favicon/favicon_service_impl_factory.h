// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FAVICON_FAVICON_SERVICE_IMPL_FACTORY_H_
#define WEBLAYER_BROWSER_FAVICON_FAVICON_SERVICE_IMPL_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace weblayer {

class FaviconServiceImpl;

// BrowserContextKeyedServiceFactory for getting the FaviconServiceImpl.
class FaviconServiceImplFactory : public BrowserContextKeyedServiceFactory {
 public:
  FaviconServiceImplFactory(const FaviconServiceImplFactory&) = delete;
  FaviconServiceImplFactory& operator=(const FaviconServiceImplFactory&) =
      delete;

  // Off the record profiles do not have a FaviconServiceImpl.
  static FaviconServiceImpl* GetForBrowserContext(
      content::BrowserContext* browser_context);

  // Returns the FaviconServiceFactory singleton.
  static FaviconServiceImplFactory* GetInstance();

 private:
  friend class base::NoDestructor<FaviconServiceImplFactory>;

  FaviconServiceImplFactory();
  ~FaviconServiceImplFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FAVICON_FAVICON_SERVICE_IMPL_FACTORY_H_
