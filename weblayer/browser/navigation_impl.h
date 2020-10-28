// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NAVIGATION_IMPL_H_
#define WEBLAYER_BROWSER_NAVIGATION_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_controller.h"
#include "weblayer/public/navigation.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace content {
class NavigationHandle;
}

namespace embedder_support {
class WebResourceResponse;
}

namespace weblayer {

class NavigationImpl : public Navigation {
 public:
  explicit NavigationImpl(content::NavigationHandle* navigation_handle);
  ~NavigationImpl() override;

  int navigation_entry_unique_id() const { return navigation_entry_unique_id_; }

  void set_should_stop_when_throttle_created() {
    should_stop_when_throttle_created_ = true;
  }
  bool should_stop_when_throttle_created() const {
    return should_stop_when_throttle_created_;
  }

  void set_safe_to_set_request_headers(bool value) {
    safe_to_set_request_headers_ = value;
  }

  void set_safe_to_set_user_agent(bool value) {
    safe_to_set_user_agent_ = value;
  }

  void set_was_stopped() { was_stopped_ = true; }

  void SetParamsToLoadWhenSafe(
      std::unique_ptr<content::NavigationController::LoadURLParams> params);
  std::unique_ptr<content::NavigationController::LoadURLParams>
  TakeParamsToLoadWhenSafe();

#if defined(OS_ANDROID)
  void SetJavaNavigation(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_navigation);
  int GetState(JNIEnv* env) { return static_cast<int>(GetState()); }
  base::android::ScopedJavaLocalRef<jstring> GetUri(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobjectArray> GetRedirectChain(JNIEnv* env);
  int GetHttpStatusCode(JNIEnv* env) { return GetHttpStatusCode(); }
  bool IsSameDocument(JNIEnv* env) { return IsSameDocument(); }
  bool IsErrorPage(JNIEnv* env) { return IsErrorPage(); }
  bool IsDownload(JNIEnv* env) { return IsDownload(); }
  bool WasStopCalled(JNIEnv* env) { return WasStopCalled(); }
  int GetLoadError(JNIEnv* env) { return static_cast<int>(GetLoadError()); }
  jboolean SetRequestHeader(JNIEnv* env,
                            const base::android::JavaParamRef<jstring>& name,
                            const base::android::JavaParamRef<jstring>& value);
  jboolean SetUserAgentString(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& value);
  jboolean IsPageInitiated(JNIEnv* env) { return IsPageInitiated(); }
  jboolean IsReload(JNIEnv* env) { return IsReload(); }

  void SetResponse(
      std::unique_ptr<embedder_support::WebResourceResponse> response);
  std::unique_ptr<embedder_support::WebResourceResponse> TakeResponse();

  base::android::ScopedJavaGlobalRef<jobject> java_navigation() {
    return java_navigation_;
  }
#endif

  // Navigation implementation:
  GURL GetURL() override;
  const std::vector<GURL>& GetRedirectChain() override;
  NavigationState GetState() override;
  int GetHttpStatusCode() override;
  bool IsSameDocument() override;
  bool IsErrorPage() override;
  bool IsDownload() override;
  bool WasStopCalled() override;
  LoadError GetLoadError() override;
  void SetRequestHeader(const std::string& name,
                        const std::string& value) override;
  void SetUserAgentString(const std::string& value) override;
  bool IsPageInitiated() override;
  bool IsReload() override;

 private:
  content::NavigationHandle* navigation_handle_;

  // The NavigationEntry's unique ID for this navigation, or -1 if there isn't
  // one.
  int navigation_entry_unique_id_ = -1;

  // Used to delay calling Stop() until safe. See
  // NavigationControllerImpl::NavigationThrottleImpl for details.
  bool should_stop_when_throttle_created_ = false;

  // Whether SetRequestHeader() is allowed at this time.
  bool safe_to_set_request_headers_ = false;

  // Whether SetUserAgentString() is allowed at this time.
  bool safe_to_set_user_agent_ = false;

  // Whether NavigationController::Stop() was called for this navigation.
  bool was_stopped_ = false;

#if defined(OS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_navigation_;
  std::unique_ptr<embedder_support::WebResourceResponse> response_;
#endif

  // Used to delay loading until safe. In particular, if Navigate() is called
  // from NavigationStarted(), then the parameters are captured and the
  // navigation started later on. The delaying is necessary as content is not
  // reentrant, and this triggers some amount of reentrancy.
  std::unique_ptr<content::NavigationController::LoadURLParams>
      scheduled_load_params_;

  DISALLOW_COPY_AND_ASSIGN(NavigationImpl);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NAVIGATION_IMPL_H_
