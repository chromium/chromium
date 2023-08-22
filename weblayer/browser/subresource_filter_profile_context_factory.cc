// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/subresource_filter_profile_context_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

namespace weblayer {

// static
subresource_filter::SubresourceFilterProfileContext*
SubresourceFilterProfileContextFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<subresource_filter::SubresourceFilterProfileContext*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 true /* create */));
}

// static
SubresourceFilterProfileContextFactory*
SubresourceFilterProfileContextFactory::GetInstance() {
  static base::NoDestructor<SubresourceFilterProfileContextFactory> factory;
  return factory.get();
}

SubresourceFilterProfileContextFactory::SubresourceFilterProfileContextFactory()
    : BrowserContextKeyedServiceFactory(
          "SubresourceFilterProfileContext",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

std::unique_ptr<KeyedService>
SubresourceFilterProfileContextFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<subresource_filter::SubresourceFilterProfileContext>(
      HostContentSettingsMapFactory::GetForBrowserContext(context));
}

content::BrowserContext*
SubresourceFilterProfileContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
