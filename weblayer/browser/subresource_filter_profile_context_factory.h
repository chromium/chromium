// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_SUBRESOURCE_FILTER_PROFILE_CONTEXT_FACTORY_H_
#define WEBLAYER_BROWSER_SUBRESOURCE_FILTER_PROFILE_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace subresource_filter {
class SubresourceFilterProfileContext;
}

namespace weblayer {

// This class is responsible for instantiating a profile-scoped context for
// subresource filtering.
class SubresourceFilterProfileContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static SubresourceFilterProfileContextFactory* GetInstance();
  static subresource_filter::SubresourceFilterProfileContext*
  GetForBrowserContext(content::BrowserContext* browser_context);

  SubresourceFilterProfileContextFactory(
      const SubresourceFilterProfileContextFactory&) = delete;
  SubresourceFilterProfileContextFactory& operator=(
      const SubresourceFilterProfileContextFactory&) = delete;

 private:
  friend class base::NoDestructor<SubresourceFilterProfileContextFactory>;

  SubresourceFilterProfileContextFactory();

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_SUBRESOURCE_FILTER_PROFILE_CONTEXT_FACTORY_H_
