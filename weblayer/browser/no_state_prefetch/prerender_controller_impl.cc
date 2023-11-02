// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/no_state_prefetch/prerender_controller_impl.h"

#include "build/build_config.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_manager_factory.h"

#if BUILDFLAG(IS_ANDROID)
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

#if BUILDFLAG(IS_ANDROID)
void PrerenderControllerImpl::Prerender(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& url) {
  Prerender(GURL(ConvertJavaStringToUTF8(url)));
}
#endif

void PrerenderControllerImpl::Prerender(const GURL& url) {
  auto* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(browser_context_);
  DCHECK(no_state_prefetch_manager);

  // The referrer parameter results in a header being set that lets the server
  // serving the URL being prefetched see where the request originated. It's an
  // optional header, it's okay to skip setting it here. SessionStorageNamespace
  // isn't necessary for NoStatePrefetch, so it's okay to pass in a nullptr.
  // NoStatePrefetchManager uses default bounds  if the one provided is empty.
  no_state_prefetch_manager->StartPrefetchingFromExternalRequest(
      url, content::Referrer(), /* session_storage_namespace= */ nullptr,
      /* bounds= */ gfx::Rect());
}

void PrerenderControllerImpl::DestroyAllContents() {
  auto* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(browser_context_);
  DCHECK(no_state_prefetch_manager);

  no_state_prefetch_manager->DestroyAllContents(
      prerender::FINAL_STATUS_APP_TERMINATING);
}

}  // namespace weblayer
