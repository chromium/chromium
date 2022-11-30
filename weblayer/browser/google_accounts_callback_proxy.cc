// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/google_accounts_callback_proxy.h"

#include "base/android/jni_string.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "weblayer/browser/java/jni/GoogleAccountsCallbackProxy_jni.h"
#include "weblayer/public/tab.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;

namespace weblayer {

GoogleAccountsCallbackProxy::GoogleAccountsCallbackProxy(JNIEnv* env,
                                                         jobject obj,
                                                         Tab* tab)
    : tab_(tab), java_impl_(env, obj) {
  tab_->SetGoogleAccountsDelegate(this);
}

GoogleAccountsCallbackProxy::~GoogleAccountsCallbackProxy() {
  tab_->SetGoogleAccountsDelegate(nullptr);
}

void GoogleAccountsCallbackProxy::OnGoogleAccountsRequest(
    const signin::ManageAccountsParams& params) {
  JNIEnv* env = AttachCurrentThread();
  Java_GoogleAccountsCallbackProxy_onGoogleAccountsRequest(
      env, java_impl_, params.service_type,
      ConvertUTF8ToJavaString(env, params.email),
      ConvertUTF8ToJavaString(env, params.continue_url), params.is_same_tab);
}

std::string GoogleAccountsCallbackProxy::GetGaiaId() {
  return base::android::ConvertJavaStringToUTF8(
      Java_GoogleAccountsCallbackProxy_getGaiaId(AttachCurrentThread(),
                                                 java_impl_));
}

static jlong JNI_GoogleAccountsCallbackProxy_CreateGoogleAccountsCallbackProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong tab) {
  return reinterpret_cast<jlong>(
      new GoogleAccountsCallbackProxy(env, proxy, reinterpret_cast<Tab*>(tab)));
}

static void JNI_GoogleAccountsCallbackProxy_DeleteGoogleAccountsCallbackProxy(
    JNIEnv* env,
    jlong proxy) {
  delete reinterpret_cast<GoogleAccountsCallbackProxy*>(proxy);
}

}  // namespace weblayer
