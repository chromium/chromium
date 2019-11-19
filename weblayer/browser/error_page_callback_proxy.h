// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_ERROR_PAGE_CALLBACK_PROXY_H_
#define WEBLAYER_BROWSER_ERROR_PAGE_CALLBACK_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "weblayer/public/error_page_delegate.h"

namespace weblayer {

class Tab;

// ErrorPageCallbackProxy forwards all ErrorPageDelegate functions to the Java
// side. There is one ErrorPageCallbackProxy per Tab.
class ErrorPageCallbackProxy : public ErrorPageDelegate {
 public:
  ErrorPageCallbackProxy(JNIEnv* env, jobject obj, Tab* tab);
  ~ErrorPageCallbackProxy() override;

  // ErrorPageDelegate:
  bool OnBackToSafety() override;

 private:
  Tab* tab_;
  base::android::ScopedJavaGlobalRef<jobject> java_impl_;

  DISALLOW_COPY_AND_ASSIGN(ErrorPageCallbackProxy);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_ERROR_PAGE_CALLBACK_PROXY_H_
