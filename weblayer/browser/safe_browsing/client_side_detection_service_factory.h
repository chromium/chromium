// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_FACTORY_H_
#define WEBLAYER_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}

namespace safe_browsing {
class ClientSideDetectionService;
}

namespace weblayer {

// Singleton that owns ClientSideDetectionServiceFactory objects and associates
// them them with BrowserContextImpl instances.
class ClientSideDetectionServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  ClientSideDetectionServiceFactory(const ClientSideDetectionServiceFactory&) =
      delete;
  ClientSideDetectionServiceFactory& operator=(
      const ClientSideDetectionServiceFactory&) = delete;

  // Creates the service if it doesn't exist already for the given
  // |browser_context|. If the service already exists, return its pointer.
  static safe_browsing::ClientSideDetectionService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  // Get the singleton instance.
  static ClientSideDetectionServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ClientSideDetectionServiceFactory>;

  ClientSideDetectionServiceFactory();
  ~ClientSideDetectionServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_SERVICE_FACTORY_H_
