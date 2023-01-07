// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_impl.h"

#include "build/build_config.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "weblayer/browser/navigation_ui_data_impl.h"
#include "weblayer/browser/page_impl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/embedder_support/android/util/web_resource_response.h"
#include "weblayer/browser/java/jni/NavigationImpl_jni.h"
#endif

#if BUILDFLAG(IS_ANDROID)
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
#if BUILDFLAG(IS_ANDROID)
  if (java_navigation_) {
    Java_NavigationImpl_onNativeDestroyed(AttachCurrentThread(),
                                          java_navigation_);
  }
#endif
}

#if BUILDFLAG(IS_ANDROID)
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

ScopedJavaLocalRef<jobjectArray> NavigationImpl::GetResponseHeaders(
    JNIEnv* env) {
  std::vector<std::string> jni_headers;
  auto* headers = GetResponseHeaders();
  if (headers) {
    size_t iterator = 0;
    std::string name;
    std::string value;
    while (headers->EnumerateHeaderLines(&iterator, &name, &value)) {
      jni_headers.push_back(name);
      jni_headers.push_back(value);
    }
  }

  return base::android::ToJavaArrayOfStrings(env, jni_headers);
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

jboolean NavigationImpl::DisableNetworkErrorAutoReload(JNIEnv* env) {
  if (!safe_to_disable_network_error_auto_reload_)
    return false;
  DisableNetworkErrorAutoReload();
  return true;
}

jboolean NavigationImpl::DisableIntentProcessing(JNIEnv* env) {
  if (!safe_to_disable_intent_processing_)
    return false;
  disable_intent_processing_ = true;
  return true;
}

jboolean NavigationImpl::AreIntentLaunchesAllowedInBackground(JNIEnv* env) {
  NavigationUIDataImpl* navigation_ui_data = static_cast<NavigationUIDataImpl*>(
      navigation_handle_->GetNavigationUIData());

  if (!navigation_ui_data)
    return false;

  return navigation_ui_data->are_intent_launches_allowed_in_background();
}

base::android::ScopedJavaLocalRef<jstring> NavigationImpl::GetReferrer(
    JNIEnv* env) {
  return ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(env, GetReferrer().spec()));
}

jlong NavigationImpl::GetPage(JNIEnv* env) {
  if (!safe_to_get_page_)
    return -1;
  return reinterpret_cast<intptr_t>(GetPage());
}

jint NavigationImpl::GetNavigationEntryOffset(JNIEnv* env) {
  return GetNavigationEntryOffset();
}

jboolean NavigationImpl::WasFetchedFromCache(JNIEnv* env) {
  return WasFetchedFromCache();
}

void NavigationImpl::SetResponse(
    std::unique_ptr<embedder_support::WebResourceResponse> response) {
  response_ = std::move(response);
}

std::unique_ptr<embedder_support::WebResourceResponse>
NavigationImpl::TakeResponse() {
  return std::move(response_);
}

void NavigationImpl::SetJavaNavigation(
    const base::android::ScopedJavaGlobalRef<jobject>& java_navigation) {
  // SetJavaNavigation() should only be called once.
  DCHECK(!java_navigation_);
  java_navigation_ = java_navigation;
}

#endif

bool NavigationImpl::IsPageInitiated() {
  return navigation_handle_->IsRendererInitiated();
}

bool NavigationImpl::IsReload() {
  return navigation_handle_->GetReloadType() != content::ReloadType::NONE;
}

bool NavigationImpl::IsServedFromBackForwardCache() {
  return navigation_handle_->IsServedFromBackForwardCache();
}

Page* NavigationImpl::GetPage() {
  if (!safe_to_get_page_)
    return nullptr;

  return PageImpl::GetForPage(
      navigation_handle_->GetRenderFrameHost()->GetPage());
}

int NavigationImpl::GetNavigationEntryOffset() {
  return navigation_handle_->GetNavigationEntryOffset();
}

bool NavigationImpl::WasFetchedFromCache() {
  return navigation_handle_->WasResponseCached();
}

GURL NavigationImpl::GetURL() {
  return navigation_handle_->GetURL();
}

const std::vector<GURL>& NavigationImpl::GetRedirectChain() {
  return navigation_handle_->GetRedirectChain();
}

NavigationState NavigationImpl::GetState() {
  if (navigation_handle_->IsErrorPage() || navigation_handle_->IsDownload() ||
      (finished_ && !navigation_handle_->HasCommitted()))
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

const net::HttpResponseHeaders* NavigationImpl::GetResponseHeaders() {
  return navigation_handle_->GetResponseHeaders();
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

bool NavigationImpl::IsKnownProtocol() {
  return !navigation_handle_->IsExternalProtocol();
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

  // The safe browsing navigation throttle fails navigations with
  // ERR_BLOCKED_BY_CLIENT when showing safe browsing interstitials.
  if (error_code == net::ERR_BLOCKED_BY_CLIENT)
    return kSafeBrowsingError;

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
  // By default renderer initiated navigations inherit the user-agent override
  // of the current NavigationEntry. But we don't want this per-navigation UA to
  // be inherited.
  navigation_handle_->GetWebContents()
      ->SetRendererInitiatedUserAgentOverrideOption(
          content::NavigationController::UA_OVERRIDE_FALSE);
  navigation_handle_->GetWebContents()->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly(value),
      /* override_in_new_tabs */ false);
  navigation_handle_->SetIsOverridingUserAgent(!value.empty());
  set_user_agent_string_called_ = true;
}

void NavigationImpl::DisableNetworkErrorAutoReload() {
  DCHECK(safe_to_disable_network_error_auto_reload_);
  disable_network_error_auto_reload_ = true;
}

bool NavigationImpl::IsFormSubmission() {
  return navigation_handle_->IsFormSubmission();
}

GURL NavigationImpl::GetReferrer() {
  return navigation_handle_->GetReferrer().url;
}

#if BUILDFLAG(IS_ANDROID)
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
