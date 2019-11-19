// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/navigation_impl.h"

#include "content/public/browser/navigation_handle.h"
#include "net/base/net_errors.h"

#if defined(OS_ANDROID)
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "weblayer/browser/java/jni/NavigationImpl_jni.h"
#endif

#if defined(OS_ANDROID)
using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
#endif

namespace weblayer {

NavigationImpl::NavigationImpl(content::NavigationHandle* navigation_handle)
    : navigation_handle_(navigation_handle) {}

NavigationImpl::~NavigationImpl() {
#if defined(OS_ANDROID)
  if (java_navigation_) {
    Java_NavigationImpl_onNativeDestroyed(AttachCurrentThread(),
                                          java_navigation_);
  }
#endif
}

#if defined(OS_ANDROID)
void NavigationImpl::SetJavaNavigation(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_navigation) {
  java_navigation_.Reset(env, java_navigation);
}

ScopedJavaLocalRef<jstring> NavigationImpl::GetUri(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return ScopedJavaLocalRef<jstring>(
      base::android::ConvertUTF8ToJavaString(env, GetURL().spec()));
}

ScopedJavaLocalRef<jobjectArray> NavigationImpl::GetRedirectChain(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  std::vector<std::string> jni_redirects;
  for (const GURL& redirect : GetRedirectChain())
    jni_redirects.push_back(redirect.spec());
  return base::android::ToJavaArrayOfStrings(env, jni_redirects);
}

#endif

GURL NavigationImpl::GetURL() {
  return navigation_handle_->GetURL();
}

const std::vector<GURL>& NavigationImpl::GetRedirectChain() {
  return navigation_handle_->GetRedirectChain();
}

NavigationState NavigationImpl::GetState() {
  if (navigation_handle_->IsErrorPage())
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

}  // namespace weblayer
