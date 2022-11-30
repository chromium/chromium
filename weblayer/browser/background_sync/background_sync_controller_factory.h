// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_FACTORY_H_
#define WEBLAYER_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class BackgroundSyncControllerImpl;

namespace content {
class BrowserContext;
}

namespace weblayer {

// Creates and maintains a BackgroundSyncController instance per BrowserContext.
class BackgroundSyncControllerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static BackgroundSyncControllerImpl* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static BackgroundSyncControllerFactory* GetInstance();

  BackgroundSyncControllerFactory(const BackgroundSyncControllerFactory&) =
      delete;
  BackgroundSyncControllerFactory& operator=(
      const BackgroundSyncControllerFactory&) = delete;

 private:
  friend class base::NoDestructor<BackgroundSyncControllerFactory>;

  BackgroundSyncControllerFactory();
  ~BackgroundSyncControllerFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_CONTROLLER_FACTORY_H_
