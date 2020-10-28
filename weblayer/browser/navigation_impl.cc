// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_impl.h"

#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/embedder_support/android/util/web_resource_response.h"
#include "weblayer/browser/java/jni/NavigationImpl_jni.h"
#endif

#if defined(OS_ANDROID)
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
#endif

namespace weblayer {

NavigationImpl::NavigationImpl(content::NavigationHandle* navigation_handle)
    : navigation_handle_(navigation_handle) {
  auto* navigation_entry = navigation_handle->GetNavigationEntry();
  if (navigation_entry &&
      navigation_entry->GetURL() == navigation_handle->GetURL()) {
    navigation_entry_unique_id_ = navigation_entry->GetUniqueID();
  }
}

NavigationImpl::~NavigationImpl() {
#if defined(OS_ANDROID)
  if (java_navigation_) {
    Java_NavigationImpl_onNativeDestroyed(AttachCurrentThread(),
                                          java_navigation_);
  }
#endif
}

void NavigationImpl::SetParamsToLoadWhenSafe(
    std::unique_ptr<content::NavigationController::LoadURLParams> params) {
  scheduled_load_params_ = std::move(params);
}

std::unique_ptr<content::NavigationController::LoadURLParams>
NavigationImpl::TakeParamsToLoadWhenSafe() {
  return std::move(scheduled_load_params_);
}

#if defined(OS_ANDROID)
void NavigationImpl::SetJavaNavigation(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_navigation) {
  java_navigation_ = java_navigation;
}

ScopedJavaLocalRef<jstring> NavigationImpl::GetUri(JNIEnv* env) {
  return ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(env, GetURL().spec()));
}

ScopedJavaLocalRef<jobjectArray> NavigationImpl::GetRedirectChain(JNIEnv* env) {
  std::vector<std::string> jni_redirects;
  for (const GURL& redirect : GetRedirectChain())
    jni_redirects.push_back(redirect.spec());
  return base::android::ToJavaArrayOfStrings(env, jni_redirects);
}

jboolean NavigationImpl::SetRequestHeader(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& name,
    const base::android::JavaParamRef<jstring>& value) {
  if (!safe_to_set_request_headers_)
    return false;

  SetRequestHeader(ConvertJavaStringToUTF8(name),
                   ConvertJavaStringToUTF8(value));
  return true;
}

jboolean NavigationImpl::SetUserAgentString(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& value) {
  if (!safe_to_set_user_agent_)
    return false;
  SetUserAgentString(ConvertJavaStringToUTF8(value));
  return true;
}

void NavigationImpl::SetResponse(
    std::unique_ptr<embedder_support::WebResourceResponse> response) {
  response_ = std::move(response);
}

std::unique_ptr<embedder_support::WebResourceResponse>
NavigationImpl::TakeResponse() {
  return std::move(response_);
}

#endif

bool NavigationImpl::IsPageInitiated() {
  return navigation_handle_->IsRendererInitiated();
}

bool NavigationImpl::IsReload() {
  return navigation_handle_->GetReloadType() != content::ReloadType::NONE;
}

GURL NavigationImpl::GetURL() {
  return navigation_handle_->GetURL();
}

const std::vector<GURL>& NavigationImpl::GetRedirectChain() {
  return navigation_handle_->GetRedirectChain();
}

NavigationState NavigationImpl::GetState() {
  if (navigation_handle_->IsErrorPage() || navigation_handle_->IsDownload())
    return NavigationState::kFailed;
  if (navigation_handle_->HasCommitted())
    return NavigationState::kComplete;
  if (navigation_handle_->GetResponseHeaders())
    return NavigationState::kReceivingBytes;
  return NavigationState::kWaitingResponse;
}

int NavigationImpl::GetHttpStatusCode() {
  auto* response_headers = navigation_handle_->GetResponseHeaders();
  return response_headers ? response_headers->response_code() : 0;
}

bool NavigationImpl::IsSameDocument() {
  return navigation_handle_->IsSameDocument();
}

bool NavigationImpl::IsErrorPage() {
  return navigation_handle_->IsErrorPage();
}

bool NavigationImpl::IsDownload() {
  return navigation_handle_->IsDownload();
}

bool NavigationImpl::WasStopCalled() {
  return was_stopped_;
}

Navigation::LoadError NavigationImpl::GetLoadError() {
  auto error_code = navigation_handle_->GetNetErrorCode();
  if (auto* response_headers = navigation_handle_->GetResponseHeaders()) {
    auto response_code = response_headers->response_code();
    if (response_code >= 400 && response_code < 500)
      return kHttpClientError;
    if (response_code >= 500 && response_code < 600)
      return kHttpServerError;
  }

  if (error_code == net::OK)
    return kNoError;

  if (net::IsCertificateError(error_code))
    return kSSLError;

  if (error_code <= -100 && error_code > -200)
    return kConnectivityError;

  return kOtherError;
}

void NavigationImpl::SetRequestHeader(const std::string& name,
                                      const std::string& value) {
  if (base::ToLowerASCII(name) == "referer") {
    // The referrer needs to be special cased as content maintains it
    // separately.
    auto referrer = blink::mojom::Referrer::New();
    referrer->url = GURL(value);
    referrer->policy = network::mojom::ReferrerPolicy::kDefault;
    navigation_handle_->SetReferrer(std::move(referrer));
  } else {
    // Any headers coming from the client should be exempt from CORS checks.
    navigation_handle_->SetCorsExemptRequestHeader(name, value);
  }
}

void NavigationImpl::SetUserAgentString(const std::string& value) {
  DCHECK(safe_to_set_user_agent_);
  navigation_handle_->GetWebContents()->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly(value),
      /* override_in_new_tabs */ false);
  navigation_handle_->SetIsOverridingUserAgent(!value.empty());
}

#if defined(OS_ANDROID)
static jboolean JNI_NavigationImpl_IsValidRequestHeaderName(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& name) {
  return net::HttpUtil::IsValidHeaderName(ConvertJavaStringToUTF8(name));
}

static jboolean JNI_NavigationImpl_IsValidRequestHeaderValue(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& value) {
  return net::HttpUtil::IsValidHeaderValue(ConvertJavaStringToUTF8(value));
}
#endif

}  // namespace weblayer
