// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_HOST_FACTORY_H_
#define WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_HOST_FACTORY_H_

#include <memory>
#include <string>

namespace weblayer {

class WebMessageHost;
class WebMessageReplyProxy;

// Creates a WebMessageHost in response to a page interacting with the object
// registered by way of Tab::AddWebMessageHostFactory(). A WebMessageHost is
// created for every page that matches the parameters of
// AddWebMessageHostFactory().
class WebMessageHostFactory {
 public:
  virtual ~WebMessageHostFactory() = default;

  // The returned object is destroyed when the corresponding renderer has
  // been destroyed. |proxy| may be used to send messages to the page and is
  // valid for the life of the WebMessageHost.
  virtual std::unique_ptr<WebMessageHost> CreateHost(
      const std::string& origin_string,
      bool is_main_frame,
      WebMessageReplyProxy* proxy) = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_HOST_FACTORY_H_
