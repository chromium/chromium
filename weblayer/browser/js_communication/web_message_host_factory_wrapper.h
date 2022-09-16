// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_JS_COMMUNICATION_WEB_MESSAGE_HOST_FACTORY_WRAPPER_H_
#define WEBLAYER_BROWSER_JS_COMMUNICATION_WEB_MESSAGE_HOST_FACTORY_WRAPPER_H_

#include "components/js_injection/browser/web_message_host_factory.h"

namespace weblayer {

class WebMessageHostFactory;

// Provides an implementation of js_injection::WebMessageHostFactory that
// wraps the corresponding WebLayer type.
class WebMessageHostFactoryWrapper
    : public js_injection::WebMessageHostFactory {
 public:
  explicit WebMessageHostFactoryWrapper(
      std::unique_ptr<weblayer::WebMessageHostFactory> factory);
  WebMessageHostFactoryWrapper(const WebMessageHostFactoryWrapper&) = delete;
  WebMessageHostFactoryWrapper& operator=(const WebMessageHostFactoryWrapper&) =
      delete;
  ~WebMessageHostFactoryWrapper() override;

  // js_injection::WebMessageHostFactory:
  std::unique_ptr<js_injection::WebMessageHost> CreateHost(
      const std::string& origin_string,
      bool is_main_frame,
      js_injection::WebMessageReplyProxy* proxy) override;

 private:
  std::unique_ptr<weblayer::WebMessageHostFactory> factory_;
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_JS_COMMUNICATION_WEB_MESSAGE_HOST_FACTORY_WRAPPER_H_
