// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_FAVICON_FAVICON_CALLBACK_PROXY_H_
#define WEBLAYER_BROWSER_FAVICON_FAVICON_CALLBACK_PROXY_H_

#include <memory>

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "weblayer/public/favicon_fetcher_delegate.h"

namespace weblayer {

class FaviconFetcher;
class Tab;

// FullscreenCallbackProxy forwards all FullscreenDelegate functions to the
// Java side. There is at most one FullscreenCallbackProxy per Tab.
class FaviconCallbackProxy : public FaviconFetcherDelegate {
 public:
  FaviconCallbackProxy(JNIEnv* env, jobject obj, Tab* tab);
  FaviconCallbackProxy(const FaviconCallbackProxy&) = delete;
  FaviconCallbackProxy& operator=(const FaviconCallbackProxy&) = delete;
  ~FaviconCallbackProxy() override;

  // FaviconFetcherDelegate:
  void OnFaviconChanged(const gfx::Image& image) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_proxy_;
  std::unique_ptr<FaviconFetcher> favicon_fetcher_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_FAVICON_FAVICON_CALLBACK_PROXY_H_
