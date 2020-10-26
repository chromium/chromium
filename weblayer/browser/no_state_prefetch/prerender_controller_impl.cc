// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/no_state_prefetch/prerender_controller_impl.h"

#include "build/build_config.h"
#include "components/prerender/browser/prerender_handle.h"
#include "components/prerender/browser/prerender_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "weblayer/browser/no_state_prefetch/prerender_manager_factory.h"

#if defined(OS_ANDROID)
#include "base/android/jni_string.h"
#include "weblayer/browser/java/jni/PrerenderControllerImpl_jni.h"
#endif

namespace weblayer {

PrerenderControllerImpl::PrerenderControllerImpl(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(browser_context_);
}

PrerenderControllerImpl::~PrerenderControllerImpl() = default;

#if defined(OS_ANDROID)
void PrerenderControllerImpl::Prerender(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& url) {
  Prerender(GURL(ConvertJavaStringToUTF8(url)));
}
#endif

void PrerenderControllerImpl::Prerender(const GURL& url) {
  auto* prerender_manager =
      PrerenderManagerFactory::GetForBrowserContext(browser_context_);
  DCHECK(prerender_manager);

  // The referrer parameter results in a header being set that lets the server
  // serving the URL being prerendered see where the request originated. It's
  // an optional header, it's okay to skip setting it here.
  // SessionStorageNamespace isn't necessary for NoStatePrefetch, so it's okay
  // to pass in a nullptr.
  // PrerenderManager uses default bounds  if the one provided is empty.
  prerender_manager->AddPrerenderFromExternalRequest(
      url, content::Referrer(), /* session_storage_namespace= */ nullptr,
      /* bounds= */ gfx::Rect());
}

}  // namespace weblayer
