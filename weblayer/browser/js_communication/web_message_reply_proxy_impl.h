// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_JS_COMMUNICATION_WEB_MESSAGE_REPLY_PROXY_IMPL_H_
#define WEBLAYER_BROWSER_JS_COMMUNICATION_WEB_MESSAGE_REPLY_PROXY_IMPL_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/js_injection/browser/web_message_host.h"
#include "weblayer/public/js_communication/web_message_host.h"

namespace weblayer {

class WebMessageReplyProxy;

// Created only on the Android side to support post-message.
// WebMessageReplyProxyImpl creates the Java WebMessageReplyProxy that is then
// sent over to the client side for communication with the page.
class WebMessageReplyProxyImpl : public WebMessageHost {
 public:
  WebMessageReplyProxyImpl(int id,
                           base::android::ScopedJavaGlobalRef<jobject> client,
                           const std::string& origin_string,
                           bool is_main_frame,
                           WebMessageReplyProxy* reply_proxy);
  WebMessageReplyProxyImpl(const WebMessageReplyProxyImpl&) = delete;
  WebMessageReplyProxyImpl& operator=(const WebMessageReplyProxyImpl&) = delete;
  ~WebMessageReplyProxyImpl() override;

  void PostMessage(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& message_contents);
  bool IsActive(JNIEnv* env);

  // WebMessageHost:
  void OnPostMessage(std::unique_ptr<WebMessage> message) override;
  void OnBackForwardCacheStateChanged() override;

 private:
  raw_ptr<WebMessageReplyProxy> reply_proxy_;

  // The Java WebMessageReplyProxy.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_JS_COMMUNICATION_WEB_MESSAGE_REPLY_PROXY_IMPL_H_
