// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ORIGIN_TRIALS_FACTORY_H_
#define WEBLAYER_BROWSER_ORIGIN_TRIALS_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace origin_trials {
class OriginTrials;
}  // namespace origin_trials

namespace weblayer {

class OriginTrialsFactory : public BrowserContextKeyedServiceFactory {
 public:
  OriginTrialsFactory(const OriginTrialsFactory&) = delete;
  OriginTrialsFactory& operator=(const OriginTrialsFactory&) = delete;

  static origin_trials::OriginTrials* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static OriginTrialsFactory* GetInstance();

 private:
  friend class base::NoDestructor<OriginTrialsFactory>;

  OriginTrialsFactory();
  ~OriginTrialsFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ORIGIN_TRIALS_FACTORY_H_
