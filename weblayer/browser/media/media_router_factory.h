// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_MEDIA_MEDIA_ROUTER_FACTORY_H_
#define WEBLAYER_BROWSER_MEDIA_MEDIA_ROUTER_FACTORY_H_

#include "components/media_router/browser/media_router_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {
class BrowserContext;
}

namespace weblayer {

class MediaRouterFactory : public media_router::MediaRouterFactory {
 public:
  MediaRouterFactory(const MediaRouterFactory&) = delete;
  MediaRouterFactory& operator=(const MediaRouterFactory&) = delete;

  static MediaRouterFactory* GetInstance();

  // Performs platform and WebLayer-specific initialization for media_router.
  static void DoPlatformInitIfNeeded();

 private:
  friend base::NoDestructor<MediaRouterFactory>;

  MediaRouterFactory();
  ~MediaRouterFactory() override;

  // MediaRouterFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_MEDIA_MEDIA_ROUTER_FACTORY_H_
