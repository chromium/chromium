// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/dialogs/http_auth_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"
#include "url/android/gurl_android.h"
#include "wolvic/jni_headers/HttpAuthManager_jni.h"

namespace wolvic {

HttpAuthManager::HttpAuthManager(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    bool first_auth_attempt,
    LoginAuthRequiredCallback callback)
    : callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GURL url = auth_info.challenger.GetURL().Resolve(auth_info.path);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_impl_ = Java_HttpAuthManager_create(
      env, reinterpret_cast<intptr_t>(this),
      url::GURLAndroid::FromNativeGURL(env, url), auth_info.is_proxy,
      first_auth_attempt);
}

HttpAuthManager::~HttpAuthManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CloseDialog();
  if (java_impl_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_HttpAuthManager_destroyed(env, java_impl_);
    java_impl_ = nullptr;
  }
}

void HttpAuthManager::CloseDialog() {
  if (!java_impl_)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_HttpAuthManager_closeDialog(env, java_impl_);
}

void HttpAuthManager::Proceed(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& username,
    const base::android::JavaParamRef<jstring>& password) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (callback_) {
    std::move(callback_).Run(net::AuthCredentials(
        base::android::ConvertJavaStringToUTF16(env, username),
        base::android::ConvertJavaStringToUTF16(env, password)));
  }

  CloseDialog();
}

void HttpAuthManager::Cancel(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (callback_)
    std::move(callback_).Run(absl::nullopt);

  CloseDialog();
}

}  // namespace wolvic
