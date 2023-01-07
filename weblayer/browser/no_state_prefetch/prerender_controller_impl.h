// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_CONTROLLER_IMPL_H_
#define WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "weblayer/public/prerender_controller.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

class GURL;

namespace content {
class BrowserContext;
}

namespace weblayer {

// Enables creation of a non-web initiated prerender request.
class PrerenderControllerImpl : public PrerenderController {
 public:
  explicit PrerenderControllerImpl(content::BrowserContext* browser_context);
  ~PrerenderControllerImpl() override;
  PrerenderControllerImpl(const PrerenderControllerImpl&) = delete;
  PrerenderControllerImpl& operator=(const PrerenderControllerImpl&) = delete;

#if BUILDFLAG(IS_ANDROID)
  void Prerender(JNIEnv* env, const base::android::JavaParamRef<jstring>& url);
#endif

  // PrerenderController
  void Prerender(const GURL& url) override;
  void DestroyAllContents() override;

 private:
  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NO_STATE_PREFETCH_PRERENDER_CONTROLLER_IMPL_H_
