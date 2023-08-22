// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_REDUCE_ACCEPT_LANGUAGE_FACTORY_H_
#define WEBLAYER_BROWSER_REDUCE_ACCEPT_LANGUAGE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/reduce_accept_language/browser/reduce_accept_language_service.h"
#include "content/public/browser/reduce_accept_language_controller_delegate.h"

namespace weblayer {

class ReduceAcceptLanguageFactory : public BrowserContextKeyedServiceFactory {
 public:
  static reduce_accept_language::ReduceAcceptLanguageService*
  GetForBrowserContext(content::BrowserContext* context);

  static ReduceAcceptLanguageFactory* GetInstance();

  // Non-copyable, non-moveable.
  ReduceAcceptLanguageFactory(const ReduceAcceptLanguageFactory&) = delete;
  ReduceAcceptLanguageFactory& operator=(const ReduceAcceptLanguageFactory&) =
      delete;

 private:
  friend base::NoDestructor<ReduceAcceptLanguageFactory>;

  ReduceAcceptLanguageFactory();
  ~ReduceAcceptLanguageFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_REDUCE_ACCEPT_LANGUAGE_FACTORY_H_
