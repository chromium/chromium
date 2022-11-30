// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/google_account_access_token_fetcher_proxy.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "weblayer/browser/java/jni/GoogleAccountAccessTokenFetcherProxy_jni.h"
#include "weblayer/browser/profile_impl.h"

namespace weblayer {

GoogleAccountAccessTokenFetcherProxy::GoogleAccountAccessTokenFetcherProxy(
    JNIEnv* env,
    jobject obj,
    Profile* profile)
    : java_delegate_(env, obj), profile_(profile) {
  profile_->SetGoogleAccountAccessTokenFetchDelegate(this);
}

GoogleAccountAccessTokenFetcherProxy::~GoogleAccountAccessTokenFetcherProxy() {
  profile_->SetGoogleAccountAccessTokenFetchDelegate(nullptr);
}

void GoogleAccountAccessTokenFetcherProxy::FetchAccessToken(
    const std::set<std::string>& scopes,
    OnTokenFetchedCallback callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::string> scopes_as_vector(scopes.begin(), scopes.end());

  // Copy |callback| on the heap to pass the pointer through JNI. This callback
  // will be deleted when it's run.
  jlong callback_id =
      reinterpret_cast<jlong>(new OnTokenFetchedCallback(std::move(callback)));

  Java_GoogleAccountAccessTokenFetcherProxy_fetchAccessToken(
      env, java_delegate_,
      base::android::ToJavaArrayOfStrings(env, scopes_as_vector),
      reinterpret_cast<jlong>(callback_id));
}

void GoogleAccountAccessTokenFetcherProxy::OnAccessTokenIdentifiedAsInvalid(
    const std::set<std::string>& scopes,
    const std::string& token) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::string> scopes_as_vector(scopes.begin(), scopes.end());

  Java_GoogleAccountAccessTokenFetcherProxy_onAccessTokenIdentifiedAsInvalid(
      env, java_delegate_,
      base::android::ToJavaArrayOfStrings(env, scopes_as_vector),
      base::android::ConvertUTF8ToJavaString(env, token));
}

static jlong
JNI_GoogleAccountAccessTokenFetcherProxy_CreateGoogleAccountAccessTokenFetcherProxy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& proxy,
    jlong profile) {
  return reinterpret_cast<jlong>(new GoogleAccountAccessTokenFetcherProxy(
      env, proxy, reinterpret_cast<ProfileImpl*>(profile)));
}

static void
JNI_GoogleAccountAccessTokenFetcherProxy_DeleteGoogleAccountAccessTokenFetcherProxy(
    JNIEnv* env,
    jlong proxy) {
  delete reinterpret_cast<GoogleAccountAccessTokenFetcherProxy*>(proxy);
}

static void JNI_GoogleAccountAccessTokenFetcherProxy_RunOnTokenFetchedCallback(
    JNIEnv* env,
    jlong callback_id,
    const base::android::JavaParamRef<jstring>& token) {
  std::unique_ptr<OnTokenFetchedCallback> cb(
      reinterpret_cast<OnTokenFetchedCallback*>(callback_id));
  std::move(*cb).Run(base::android::ConvertJavaStringToUTF8(env, token));
}

}  // namespace weblayer
