// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/media/media_router_factory.h"

#include "base/android/jni_android.h"
#include "base/no_destructor.h"
#include "components/media_router/browser/android/media_router_android.h"
#include "components/media_router/browser/android/media_router_dialog_controller_android.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "content/public/browser/browser_context.h"
#include "weblayer/browser/java/jni/MediaRouterClientImpl_jni.h"

namespace weblayer {

// static
MediaRouterFactory* MediaRouterFactory::GetInstance() {
  static base::NoDestructor<MediaRouterFactory> instance;
  return instance.get();
}

// static
bool MediaRouterFactory::IsFeatureEnabled() {
  static bool enabled = Java_MediaRouterClientImpl_isMediaRouterEnabled(
      base::android::AttachCurrentThread());
  return enabled;
}

// static
void MediaRouterFactory::DoPlatformInitIfNeeded() {
  static bool init_done = false;
  if (init_done)
    return;

  Java_MediaRouterClientImpl_initialize(base::android::AttachCurrentThread());

  media_router::MediaRouterDialogController::SetGetOrCreate(
      base::BindRepeating([](content::WebContents* web_contents) {
        DCHECK(web_contents);
        // This call does nothing if the controller already exists.
        media_router::MediaRouterDialogControllerAndroid::CreateForWebContents(
            web_contents);
        return static_cast<media_router::MediaRouterDialogController*>(
            media_router::MediaRouterDialogControllerAndroid::FromWebContents(
                web_contents));
      }));
  init_done = true;
}

MediaRouterFactory::MediaRouterFactory() = default;
MediaRouterFactory::~MediaRouterFactory() = default;

content::BrowserContext* MediaRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

std::unique_ptr<KeyedService>
MediaRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  std::unique_ptr<media_router::MediaRouterBase> media_router =
      std::make_unique<media_router::MediaRouterAndroid>();
  media_router->Initialize();
  return media_router;
}

}  // namespace weblayer
