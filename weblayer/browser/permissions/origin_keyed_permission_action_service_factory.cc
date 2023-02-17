// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/permissions/origin_keyed_permission_action_service_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/origin_keyed_permission_action_service.h"

namespace weblayer {

// static
permissions::OriginKeyedPermissionActionService*
OriginKeyedPermissionActionServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<permissions::OriginKeyedPermissionActionService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
OriginKeyedPermissionActionServiceFactory*
OriginKeyedPermissionActionServiceFactory::GetInstance() {
  static base::NoDestructor<OriginKeyedPermissionActionServiceFactory> factory;
  return factory.get();
}

OriginKeyedPermissionActionServiceFactory::
    OriginKeyedPermissionActionServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "OriginKeyedPermissionActionService",
          BrowserContextDependencyManager::GetInstance()) {}

OriginKeyedPermissionActionServiceFactory::
    ~OriginKeyedPermissionActionServiceFactory() = default;

KeyedService*
OriginKeyedPermissionActionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new permissions::OriginKeyedPermissionActionService();
}

}  // namespace weblayer
