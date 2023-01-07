// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_BACKGROUND_DOWNLOAD_SERVICE_FACTORY_H_
#define WEBLAYER_BROWSER_BACKGROUND_DOWNLOAD_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace download {
class BackgroundDownloadService;
}  // namespace download

namespace weblayer {

// Unlike Chrome, which can operate outside of full browser mode, WebLayer can
// assume the full BrowserContext is available. For that reason this class is a
// BrowserContextKeyedService rather than a SimpleKeyedServiceFactory.
class BackgroundDownloadServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static BackgroundDownloadServiceFactory* GetInstance();

  static download::BackgroundDownloadService* GetForBrowserContext(
      content::BrowserContext* context);

  BackgroundDownloadServiceFactory(
      const BackgroundDownloadServiceFactory& other) = delete;
  BackgroundDownloadServiceFactory& operator=(
      const BackgroundDownloadServiceFactory& other) = delete;

 private:
  friend class base::NoDestructor<BackgroundDownloadServiceFactory>;

  BackgroundDownloadServiceFactory();
  ~BackgroundDownloadServiceFactory() override = default;

  // BrowserContextKeyedService:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_BACKGROUND_DOWNLOAD_SERVICE_FACTORY_H_
