// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_
#define WEBLAYER_BROWSER_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace translate {
class TranslateAcceptLanguages;
}

namespace weblayer {

// TranslateAcceptLanguagesFactory is a way to associate a
// TranslateAcceptLanguages instance to a BrowserContext.
class TranslateAcceptLanguagesFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  TranslateAcceptLanguagesFactory(const TranslateAcceptLanguagesFactory&) =
      delete;
  TranslateAcceptLanguagesFactory& operator=(
      const TranslateAcceptLanguagesFactory&) = delete;

  static translate::TranslateAcceptLanguages* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static TranslateAcceptLanguagesFactory* GetInstance();

 private:
  friend class base::NoDestructor<TranslateAcceptLanguagesFactory>;

  TranslateAcceptLanguagesFactory();
  ~TranslateAcceptLanguagesFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_
