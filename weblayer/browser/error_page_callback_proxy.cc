// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/error_page_callback_proxy.h"

#include "base/android/jni_string.h"
#include "url/gurl.h"
#include "weblayer/browser/java/jni/ErrorPageCallbackProxy_jni.h"
#include "weblayer/browser/navigation_impl.h"
#include "weblayer/public/error_page.h"
#include "weblayer/public/tab.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace weblayer {

ErrorPageCallbackProxy::ErrorPageCallbackProxy(JNIEnv* env,
                                               jobject obj,
                                               Tab* tab)
    : tab_(tab), java_impl_(env, obj) {
  tab_->SetErrorPageDelegate(this);
}

ErrorPageCallbackProxy::~ErrorPageCallbackProxy() {
  tab_->SetErrorPageDelegate(nullptr);
}

bool ErrorPageCallbackProxy::OnBackToSafety() {
  JNIEnv* env = AttachCurrentThread();
  return Java_ErrorPageCallbackProxy_onBackToSafety(env, java_impl_);
}

std::unique_ptr<ErrorPage> ErrorPageCallbackProxy::GetErrorPageContent(
    Navigation* navigation) {
  JNIEnv* env = AttachCurrentThread();
  auto error_string = Java_ErrorPageCallbackProxy_getErrorPageContent(
      env, java_impl_,
      static_cast<NavigationImpl*>(navigation)->java_navigation());
  if (!error_string)
    return nullptr;
  auto error_page = std::make_unique<ErrorPage>();
  error_page->html = ConvertJavaStringToUTF8(env, error_string);
  return error_page;
}

static jlong JNI_ErrorPageCallbackProxy_CreateErrorPageCallbackProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong tab) {
  return reinterpret_cast<jlong>(
      new ErrorPageCallbackProxy(env, proxy, reinterpret_cast<Tab*>(tab)));
}

static void JNI_ErrorPageCallbackProxy_DeleteErrorPageCallbackProxy(
    JNIEnv* env,
    jlong proxy) {
  delete reinterpret_cast<ErrorPageCallbackProxy*>(proxy);
}

}  // namespace weblayer
