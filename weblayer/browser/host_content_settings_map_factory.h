// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_
#define WEBLAYER_BROWSER_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

class HostContentSettingsMap;

namespace weblayer {

class HostContentSettingsMapFactory
    : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  HostContentSettingsMapFactory(const HostContentSettingsMapFactory&) = delete;
  HostContentSettingsMapFactory& operator=(
      const HostContentSettingsMapFactory&) = delete;

  static HostContentSettingsMap* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static HostContentSettingsMapFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<HostContentSettingsMapFactory>;

  HostContentSettingsMapFactory();
  ~HostContentSettingsMapFactory() override;

  // RefcountedBrowserContextKeyedServiceFactory methods:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_HOST_CONTENT_SETTINGS_MAP_FACTORY_H_
