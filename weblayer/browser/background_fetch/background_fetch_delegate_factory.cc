// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/background_fetch/background_fetch_delegate_factory.h"

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "weblayer/browser/background_download_service_factory.h"
#include "weblayer/browser/background_fetch/background_fetch_delegate_impl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "weblayer/browser/android/application_info_helper.h"
#endif

namespace weblayer {

// static
BackgroundFetchDelegateImpl*
BackgroundFetchDelegateFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<BackgroundFetchDelegateImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
BackgroundFetchDelegateFactory* BackgroundFetchDelegateFactory::GetInstance() {
  return base::Singleton<BackgroundFetchDelegateFactory>::get();
}

BackgroundFetchDelegateFactory::BackgroundFetchDelegateFactory()
    : BrowserContextKeyedServiceFactory(
          "BackgroundFetchService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(BackgroundDownloadServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

BackgroundFetchDelegateFactory::~BackgroundFetchDelegateFactory() = default;

KeyedService* BackgroundFetchDelegateFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BackgroundFetchDelegateImpl(context);
}

content::BrowserContext* BackgroundFetchDelegateFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
