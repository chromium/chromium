// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_JS_COMMUNICATION_WEB_MESSAGE_HOST_FACTORY_PROXY_H_
#define WEBLAYER_BROWSER_JS_COMMUNICATION_WEB_MESSAGE_HOST_FACTORY_PROXY_H_

#include "base/android/scoped_java_ref.h"
#include "weblayer/public/js_communication/web_message_host_factory.h"

namespace weblayer {

// TabImpl, on android, creates a WebMessageHostFactoryProxy for every call
// to RegisterWebMessageCallback(). This is used to delegate the calls back to
// the Java side.
class WebMessageHostFactoryProxy : public WebMessageHostFactory {
 public:
  explicit WebMessageHostFactoryProxy(
      const base::android::JavaParamRef<jobject>& client);
  WebMessageHostFactoryProxy(const WebMessageHostFactoryProxy&) = delete;
  WebMessageHostFactoryProxy& operator=(const WebMessageHostFactoryProxy&) =
      delete;
  ~WebMessageHostFactoryProxy() override;

  // WebMessageHostFactory:
  std::unique_ptr<WebMessageHost> CreateHost(
      const std::string& origin_string,
      bool is_main_frame,
      WebMessageReplyProxy* proxy) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> client_;
  int next_id_ = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_JS_COMMUNICATION_WEB_MESSAGE_HOST_FACTORY_PROXY_H_
