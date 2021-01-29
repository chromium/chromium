// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/js_communication/web_message_host_factory_wrapper.h"

#include "components/js_injection/browser/web_message.h"
#include "components/js_injection/browser/web_message_host.h"
#include "components/js_injection/browser/web_message_reply_proxy.h"
#include "weblayer/public/js_communication/web_message.h"
#include "weblayer/public/js_communication/web_message_host.h"
#include "weblayer/public/js_communication/web_message_host_factory.h"
#include "weblayer/public/js_communication/web_message_reply_proxy.h"

namespace weblayer {
namespace {

// An implementation of js_injection::WebMessageHost that delegates to the
// corresponding WebLayer type. This also serves as the WebMessageReplyProxy
// implementation, which forwards to the js_injection implementation.
class WebMessageHostWrapper : public js_injection::WebMessageHost,
                              public WebMessageReplyProxy {
 public:
  WebMessageHostWrapper(weblayer::WebMessageHostFactory* factory,
                        const std::string& origin_string,
                        bool is_main_frame,
                        js_injection::WebMessageReplyProxy* proxy)
      : proxy_(proxy),
        connection_(factory->CreateHost(origin_string, is_main_frame, this)) {}

  // js_injection::WebMessageHost:
  void OnPostMessage(
      std::unique_ptr<js_injection::WebMessage> message) override {
    std::unique_ptr<WebMessage> m = std::make_unique<WebMessage>();
    m->message = message->message;
    connection_->OnPostMessage(std::move(m));
  }

  // WebMessageReplyProxy:
  void PostMessage(std::unique_ptr<WebMessage> message) override {
    std::unique_ptr<js_injection::WebMessage> w =
        std::make_unique<js_injection::WebMessage>();
    w->message = std::move(message->message);
    proxy_->PostMessage(std::move(w));
  }

 private:
  js_injection::WebMessageReplyProxy* proxy_;
  std::unique_ptr<weblayer::WebMessageHost> connection_;
};

}  // namespace

WebMessageHostFactoryWrapper::WebMessageHostFactoryWrapper(
    std::unique_ptr<weblayer::WebMessageHostFactory> factory)
    : factory_(std::move(factory)) {}

WebMessageHostFactoryWrapper::~WebMessageHostFactoryWrapper() = default;

std::unique_ptr<js_injection::WebMessageHost>
WebMessageHostFactoryWrapper::CreateHost(
    const std::string& origin_string,
    bool is_main_frame,
    js_injection::WebMessageReplyProxy* proxy) {
  auto wrapper = std::make_unique<WebMessageHostWrapper>(
      factory_.get(), origin_string, is_main_frame, proxy);
  return wrapper;
}

}  // namespace weblayer
