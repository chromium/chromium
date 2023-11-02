// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/background_sync/background_sync_controller_factory.h"

#include "base/no_destructor.h"
#include "components/background_sync/background_sync_controller_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "weblayer/browser/background_sync/background_sync_delegate_impl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

namespace weblayer {

// static
BackgroundSyncControllerImpl*
BackgroundSyncControllerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  // This is safe because BuildServiceInstanceFor(), which this method calls,
  // returns a pointer to a BackgroundSyncControllerImpl object.
  return static_cast<BackgroundSyncControllerImpl*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
BackgroundSyncControllerFactory*
BackgroundSyncControllerFactory::GetInstance() {
  static base::NoDestructor<BackgroundSyncControllerFactory> factory;
  return factory.get();
}

BackgroundSyncControllerFactory::BackgroundSyncControllerFactory()
    : BrowserContextKeyedServiceFactory(
          "BackgroundSyncService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

BackgroundSyncControllerFactory::~BackgroundSyncControllerFactory() = default;

KeyedService* BackgroundSyncControllerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BackgroundSyncControllerImpl(
      context, std::make_unique<BackgroundSyncDelegateImpl>(context));
}

content::BrowserContext*
BackgroundSyncControllerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Background sync operates in incognito mode, and as incognito profiles in
  // Weblayer are not tied to regular profiles, return |context| itself.
  return context;
}

}  // namespace weblayer
