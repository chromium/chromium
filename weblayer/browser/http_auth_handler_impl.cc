// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/http_auth_handler_impl.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/auth.h"
#include "url/android/gurl_android.h"
#include "weblayer/browser/java/jni/HttpAuthHandlerImpl_jni.h"
#include "weblayer/browser/tab_impl.h"

namespace weblayer {

HttpAuthHandlerImpl::HttpAuthHandlerImpl(
    const net::AuthChallengeInfo& auth_info,
    content::WebContents* web_contents,
    bool first_auth_attempt,
    LoginAuthRequiredCallback callback)
    : callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  url_ = auth_info.challenger.GetURL().Resolve(auth_info.path);

  auto* tab = TabImpl::FromWebContents(web_contents);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_impl_ = Java_HttpAuthHandlerImpl_create(
      env, reinterpret_cast<intptr_t>(this), tab->GetJavaTab(),
      url::GURLAndroid::FromNativeGURL(env, url_));
}

HttpAuthHandlerImpl::~HttpAuthHandlerImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CloseDialog();
  if (java_impl_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_HttpAuthHandlerImpl_handlerDestroyed(env, java_impl_);
    java_impl_ = nullptr;
  }
}

void HttpAuthHandlerImpl::CloseDialog() {
  if (!java_impl_)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_HttpAuthHandlerImpl_closeDialog(env, java_impl_);
}

void HttpAuthHandlerImpl::Proceed(
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

void HttpAuthHandlerImpl::Cancel(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (callback_)
    std::move(callback_).Run(absl::nullopt);

  CloseDialog();
}

}  // namespace weblayer
