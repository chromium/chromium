// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_MEDIA_MEDIA_ROUTER_FACTORY_H_
#define WEBLAYER_BROWSER_MEDIA_MEDIA_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/media_router/browser/media_router_factory.h"

namespace content {
class BrowserContext;
}

namespace weblayer {

class MediaRouterFactory : public media_router::MediaRouterFactory {
 public:
  MediaRouterFactory(const MediaRouterFactory&) = delete;
  MediaRouterFactory& operator=(const MediaRouterFactory&) = delete;

  static MediaRouterFactory* GetInstance();

  // Determines if media router related features should be enabled.
  static bool IsFeatureEnabled();

  // Performs platform and WebLayer-specific initialization for media_router.
  static void DoPlatformInitIfNeeded();

 private:
  friend base::NoDestructor<MediaRouterFactory>;

  MediaRouterFactory();
  ~MediaRouterFactory() override;

  // MediaRouterFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_MEDIA_MEDIA_ROUTER_FACTORY_H_
