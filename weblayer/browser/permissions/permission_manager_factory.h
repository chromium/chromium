// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_PERMISSIONS_PERMISSION_MANAGER_FACTORY_H_
#define WEBLAYER_BROWSER_PERMISSIONS_PERMISSION_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace permissions {
class PermissionManager;
}

namespace weblayer {

class PermissionManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  PermissionManagerFactory(const PermissionManagerFactory&) = delete;
  PermissionManagerFactory& operator=(const PermissionManagerFactory&) = delete;

  static permissions::PermissionManager* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static PermissionManagerFactory* GetInstance();

 private:
  friend class base::NoDestructor<PermissionManagerFactory>;

  PermissionManagerFactory();
  ~PermissionManagerFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_PERMISSIONS_PERMISSION_MANAGER_FACTORY_H_
