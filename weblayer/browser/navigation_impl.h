// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NAVIGATION_IMPL_H_
#define WEBLAYER_BROWSER_NAVIGATION_IMPL_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "weblayer/public/navigation.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace content {
class NavigationHandle;
}

namespace weblayer {

class NavigationImpl : public Navigation {
 public:
  explicit NavigationImpl(content::NavigationHandle* navigation_handle);
  ~NavigationImpl() override;

#if defined(OS_ANDROID)
  void SetJavaNavigation(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_navigation);
  int GetState(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj) {
    return static_cast<int>(GetState());
  }
  base::android::ScopedJavaLocalRef<jstring> GetUri(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jobjectArray> GetRedirectChain(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  int GetHttpStatusCode(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj) {
    return GetHttpStatusCode();
  }
  bool IsSameDocument(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj) {
    return IsSameDocument();
  }
  bool IsErrorPage(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj) {
    return IsErrorPage();
  }
  int GetLoadError(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj) {
    return static_cast<int>(GetLoadError());
  }

  base::android::ScopedJavaGlobalRef<jobject> java_navigation() {
    return java_navigation_;
  }
#endif

 private:
  // Navigation implementation:
  GURL GetURL() override;
  const std::vector<GURL>& GetRedirectChain() override;
  NavigationState GetState() override;
  int GetHttpStatusCode() override;
  bool IsSameDocument() override;
  bool IsErrorPage() override;
  LoadError GetLoadError() override;

  content::NavigationHandle* navigation_handle_;

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_navigation_;
#endif

  DISALLOW_COPY_AND_ASSIGN(NavigationImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NAVIGATION_IMPL_H_
