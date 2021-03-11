// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_DOWNLOAD_SERVICE_FACTORY_H_
#define WEBLAYER_BROWSER_DOWNLOAD_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace download {
class DownloadService;
}

namespace weblayer {

// Unlike Chrome, which can operate outside of full browser mode, WebLayer can
// assume the full BrowserContext is available. For that reason this class is a
// BrowserContextKeyedService rather than a SimpleKeyedServiceFactory.
class DownloadServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static DownloadServiceFactory* GetInstance();

  static download::DownloadService* GetForBrowserContext(
      content::BrowserContext* context);

  DownloadServiceFactory(const DownloadServiceFactory& other) = delete;
  DownloadServiceFactory& operator=(const DownloadServiceFactory& other) =
      delete;

 private:
  friend class base::NoDestructor<DownloadServiceFactory>;

  DownloadServiceFactory();
  ~DownloadServiceFactory() override = default;

  // BrowserContextKeyedService:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_DOWNLOAD_SERVICE_FACTORY_H_
