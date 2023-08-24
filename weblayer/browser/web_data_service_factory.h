// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEB_DATA_SERVICE_FACTORY_H_
#define WEBLAYER_BROWSER_WEB_DATA_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"

namespace weblayer {

// Singleton that owns all WebDataServiceWrappers and associates them with
// Profiles.
class WebDataServiceFactory
    : public webdata_services::WebDataServiceWrapperFactory {
 public:
  WebDataServiceFactory(const WebDataServiceFactory&) = delete;
  WebDataServiceFactory& operator=(const WebDataServiceFactory&) = delete;

  static WebDataServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<WebDataServiceFactory>;

  WebDataServiceFactory();
  ~WebDataServiceFactory() override;

  // |BrowserContextKeyedServiceFactory| methods:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEB_DATA_SERVICE_FACTORY_H_
