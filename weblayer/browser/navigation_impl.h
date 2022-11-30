// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_NAVIGATION_IMPL_H_
#define WEBLAYER_BROWSER_NAVIGATION_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "weblayer/public/navigation.h"

#if BUILDFLAG(IS_ANDROID)
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
  NavigationImpl(const NavigationImpl&) = delete;
  NavigationImpl& operator=(const NavigationImpl&) = delete;
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

  void set_safe_to_disable_network_error_auto_reload(bool value) {
    safe_to_disable_network_error_auto_reload_ = value;
  }

  void set_safe_to_disable_intent_processing(bool value) {
    safe_to_disable_intent_processing_ = value;
  }

  void set_safe_to_get_page() { safe_to_get_page_ = true; }

  void set_was_stopped() { was_stopped_ = true; }

  bool set_user_agent_string_called() { return set_user_agent_string_called_; }

  bool disable_network_error_auto_reload() {
    return disable_network_error_auto_reload_;
  }

  bool disable_intent_processing() { return disable_intent_processing_; }

  void set_finished() { finished_ = true; }

#if BUILDFLAG(IS_ANDROID)
  int GetState(JNIEnv* env) { return static_cast<int>(GetState()); }
  base::android::ScopedJavaLocalRef<jstring> GetUri(JNIEnv* env);
  base::android::ScopedJavaLocalRef<jobjectArray> GetRedirectChain(JNIEnv* env);
  int GetHttpStatusCode(JNIEnv* env) { return GetHttpStatusCode(); }
  base::android::ScopedJavaLocalRef<jobjectArray> GetResponseHeaders(
      JNIEnv* env);
  bool IsSameDocument(JNIEnv* env) { return IsSameDocument(); }
  bool IsErrorPage(JNIEnv* env) { return IsErrorPage(); }
  bool IsDownload(JNIEnv* env) { return IsDownload(); }
  bool IsKnownProtocol(JNIEnv* env) { return IsKnownProtocol(); }
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
  jboolean IsServedFromBackForwardCache(JNIEnv* env) {
    return IsServedFromBackForwardCache();
  }
  jboolean DisableNetworkErrorAutoReload(JNIEnv* env);
  jboolean DisableIntentProcessing(JNIEnv* env);
  jboolean AreIntentLaunchesAllowedInBackground(JNIEnv* env);
  jboolean IsFormSubmission(JNIEnv* env) { return IsFormSubmission(); }
  base::android::ScopedJavaLocalRef<jstring> GetReferrer(JNIEnv* env);
  jlong GetPage(JNIEnv* env);
  int GetNavigationEntryOffset(JNIEnv* env);
  jboolean WasFetchedFromCache(JNIEnv* env);

  void SetResponse(
      std::unique_ptr<embedder_support::WebResourceResponse> response);
  std::unique_ptr<embedder_support::WebResourceResponse> TakeResponse();

  void SetJavaNavigation(
      const base::android::ScopedJavaGlobalRef<jobject>& java_navigation);
  base::android::ScopedJavaGlobalRef<jobject> java_navigation() {
    return java_navigation_;
  }
#endif

  // Navigation implementation:
  GURL GetURL() override;
  const std::vector<GURL>& GetRedirectChain() override;
  NavigationState GetState() override;
  int GetHttpStatusCode() override;
  const net::HttpResponseHeaders* GetResponseHeaders() override;
  bool IsSameDocument() override;
  bool IsErrorPage() override;
  bool IsDownload() override;
  bool IsKnownProtocol() override;
  bool WasStopCalled() override;
  LoadError GetLoadError() override;
  void SetRequestHeader(const std::string& name,
                        const std::string& value) override;
  void SetUserAgentString(const std::string& value) override;
  void DisableNetworkErrorAutoReload() override;
  bool IsPageInitiated() override;
  bool IsReload() override;
  bool IsServedFromBackForwardCache() override;
  bool IsFormSubmission() override;
  GURL GetReferrer() override;
  Page* GetPage() override;
  int GetNavigationEntryOffset() override;
  bool WasFetchedFromCache() override;

 private:
  raw_ptr<content::NavigationHandle> navigation_handle_;

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

  // Whether SetUserAgentString was called.
  bool set_user_agent_string_called_ = false;

  // Whether DisableNetworkErrorAutoReload is allowed at this time.
  bool safe_to_disable_network_error_auto_reload_ = false;

  // Whether DisableIntentProcessing is allowed at this time.
  bool safe_to_disable_intent_processing_ = false;

  // Whether GetPage is allowed at this time.
  bool safe_to_get_page_ = false;

  bool disable_network_error_auto_reload_ = false;

  bool disable_intent_processing_ = false;

  // Whether this navigation has finished.
  bool finished_ = false;

#if BUILDFLAG(IS_ANDROID)
  base::android::ScopedJavaGlobalRef<jobject> java_navigation_;
  std::unique_ptr<embedder_support::WebResourceResponse> response_;
#endif
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_NAVIGATION_IMPL_H_
