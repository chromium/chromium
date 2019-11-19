// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/error_page_callback_proxy.h"

#include "url/gurl.h"
#include "weblayer/browser/java/jni/ErrorPageCallbackProxy_jni.h"
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
