// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ERROR_PAGE_CALLBACK_PROXY_H_
#define WEBLAYER_BROWSER_ERROR_PAGE_CALLBACK_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "weblayer/public/error_page_delegate.h"

namespace weblayer {

class Tab;

// ErrorPageCallbackProxy forwards all ErrorPageDelegate functions to the Java
// side. There is one ErrorPageCallbackProxy per Tab.
class ErrorPageCallbackProxy : public ErrorPageDelegate {
 public:
  ErrorPageCallbackProxy(JNIEnv* env, jobject obj, Tab* tab);

  ErrorPageCallbackProxy(const ErrorPageCallbackProxy&) = delete;
  ErrorPageCallbackProxy& operator=(const ErrorPageCallbackProxy&) = delete;

  ~ErrorPageCallbackProxy() override;

  // ErrorPageDelegate:
  bool OnBackToSafety() override;
  std::unique_ptr<ErrorPage> GetErrorPageContent(
      Navigation* navigation) override;

 private:
  raw_ptr<Tab> tab_;
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ERROR_PAGE_CALLBACK_PROXY_H_
