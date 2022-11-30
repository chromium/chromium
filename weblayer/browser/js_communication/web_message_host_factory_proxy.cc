// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/js_communication/web_message_host_factory_proxy.h"

#include "weblayer/browser/js_communication/web_message_reply_proxy_impl.h"

namespace weblayer {

WebMessageHostFactoryProxy::WebMessageHostFactoryProxy(
    const base::android::JavaParamRef<jobject>& client)
    : client_(client) {}

WebMessageHostFactoryProxy::~WebMessageHostFactoryProxy() = default;

std::unique_ptr<WebMessageHost> WebMessageHostFactoryProxy::CreateHost(
    const std::string& origin_string,
    bool is_main_frame,
    WebMessageReplyProxy* proxy) {
  return std::make_unique<WebMessageReplyProxyImpl>(
      ++next_id_, client_, origin_string, is_main_frame, proxy);
}

}  // namespace weblayer
