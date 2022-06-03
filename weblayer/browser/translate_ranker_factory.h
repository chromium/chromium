// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_TRANSLATE_RANKER_FACTORY_H_
#define WEBLAYER_BROWSER_TRANSLATE_RANKER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace translate {
class TranslateRanker;
}

namespace weblayer {

// TranslateRankerFactory is a way to associate a TranslateRanker instance to a
// BrowserContext.
class TranslateRankerFactory : public BrowserContextKeyedServiceFactory {
 public:
  TranslateRankerFactory(const TranslateRankerFactory&) = delete;
  TranslateRankerFactory& operator=(const TranslateRankerFactory&) = delete;

  static TranslateRankerFactory* GetInstance();
  static translate::TranslateRanker* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  friend class base::NoDestructor<TranslateRankerFactory>;

  TranslateRankerFactory();
  ~TranslateRankerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  // Note: In //chrome, when the service is requested for a
  // Profile in incognito mode the factory supplies the associated original
  // Profile. However, WebLayer doesn't have a concept of incognito profiles
  // being associated with regular profiles, so the service gets its own
  // instance in incognito mode.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_TRANSLATE_RANKER_FACTORY_H_
