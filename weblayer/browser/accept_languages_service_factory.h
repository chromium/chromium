// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_
#define WEBLAYER_BROWSER_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace language {
class AcceptLanguagesService;
}

namespace weblayer {

// AcceptLanguagesServiceFactory is a way to associate an
// AcceptLanguagesService instance to a BrowserContext.
class AcceptLanguagesServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  AcceptLanguagesServiceFactory(const AcceptLanguagesServiceFactory&) = delete;
  AcceptLanguagesServiceFactory& operator=(
      const AcceptLanguagesServiceFactory&) = delete;

  static language::AcceptLanguagesService* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static AcceptLanguagesServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<AcceptLanguagesServiceFactory>;

  AcceptLanguagesServiceFactory();
  ~AcceptLanguagesServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedServi> e* BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_
