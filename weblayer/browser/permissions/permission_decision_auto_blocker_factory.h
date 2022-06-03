// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_FACTORY_H_
#define WEBLAYER_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace permissions {
class PermissionDecisionAutoBlocker;
}

namespace weblayer {

class PermissionDecisionAutoBlockerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  PermissionDecisionAutoBlockerFactory(
      const PermissionDecisionAutoBlockerFactory&) = delete;
  PermissionDecisionAutoBlockerFactory& operator=(
      const PermissionDecisionAutoBlockerFactory&) = delete;

  static permissions::PermissionDecisionAutoBlocker* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static PermissionDecisionAutoBlockerFactory* GetInstance();

 private:
  friend class base::NoDestructor<PermissionDecisionAutoBlockerFactory>;

  PermissionDecisionAutoBlockerFactory();
  ~PermissionDecisionAutoBlockerFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PERMISSIONS_PERMISSION_DECISION_AUTO_BLOCKER_FACTORY_H_
