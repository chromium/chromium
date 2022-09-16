// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/js_communication/web_message_reply_proxy_impl.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "weblayer/browser/java/jni/WebMessageReplyProxyImpl_jni.h"
#include "weblayer/browser/page_impl.h"
#include "weblayer/public/js_communication/web_message.h"
#include "weblayer/public/js_communication/web_message_reply_proxy.h"

namespace weblayer {

WebMessageReplyProxyImpl::WebMessageReplyProxyImpl(
    int id,
    base::android::ScopedJavaGlobalRef<jobject> client,
    const std::string& origin_string,
    bool is_main_frame,
    WebMessageReplyProxy* reply_proxy)
    : reply_proxy_(reply_proxy) {
  auto* env = base::android::AttachCurrentThread();
  java_object_ = Java_WebMessageReplyProxyImpl_create(
      env, reinterpret_cast<intptr_t>(this), id, client, is_main_frame,
      base::android::ConvertUTF8ToJavaString(env, origin_string),
      static_cast<PageImpl&>(reply_proxy->GetPage()).java_page());
}

WebMessageReplyProxyImpl::~WebMessageReplyProxyImpl() {
  Java_WebMessageReplyProxyImpl_onNativeDestroyed(
      base::android::AttachCurrentThread(), java_object_);
}

void WebMessageReplyProxyImpl::PostMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& message_contents) {
  auto message = std::make_unique<WebMessage>();
  base::android::ConvertJavaStringToUTF16(env, message_contents,
                                          &(message->message));
  reply_proxy_->PostWebMessage(std::move(message));
}

bool WebMessageReplyProxyImpl::IsActive(JNIEnv* env) {
  return !reply_proxy_->IsInBackForwardCache();
}

void WebMessageReplyProxyImpl::OnPostMessage(
    std::unique_ptr<WebMessage> message) {
  auto* env = base::android::AttachCurrentThread();
  Java_WebMessageReplyProxyImpl_onPostMessage(
      env, java_object_,
      base::android::ConvertUTF16ToJavaString(env, message->message));
}

void WebMessageReplyProxyImpl::OnBackForwardCacheStateChanged() {
  Java_WebMessageReplyProxyImpl_onActiveStateChanged(
      base::android::AttachCurrentThread(), java_object_);
}

}  // namespace weblayer
