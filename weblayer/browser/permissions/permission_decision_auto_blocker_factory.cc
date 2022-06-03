// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/permissions/permission_decision_auto_blocker_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "weblayer/browser/host_content_settings_map_factory.h"

namespace weblayer {

// static
permissions::PermissionDecisionAutoBlocker*
PermissionDecisionAutoBlockerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<permissions::PermissionDecisionAutoBlocker*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
PermissionDecisionAutoBlockerFactory*
PermissionDecisionAutoBlockerFactory::GetInstance() {
  static base::NoDestructor<PermissionDecisionAutoBlockerFactory> factory;
  return factory.get();
}

PermissionDecisionAutoBlockerFactory::PermissionDecisionAutoBlockerFactory()
    : BrowserContextKeyedServiceFactory(
          "PermissionDecisionAutoBlocker",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
}

PermissionDecisionAutoBlockerFactory::~PermissionDecisionAutoBlockerFactory() =
    default;

KeyedService* PermissionDecisionAutoBlockerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new permissions::PermissionDecisionAutoBlocker(
      HostContentSettingsMapFactory::GetForBrowserContext(context));
}

content::BrowserContext*
PermissionDecisionAutoBlockerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace weblayer
