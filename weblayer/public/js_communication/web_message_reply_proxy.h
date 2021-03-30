// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_REPLY_PROXY_H_
#define WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_REPLY_PROXY_H_

#include <memory>

namespace weblayer {

struct WebMessage;

// Used to send messages to the page.
class WebMessageReplyProxy {
 public:
  virtual void PostMessage(std::unique_ptr<WebMessage>) = 0;

  // Returns true if the page is in the back/forward cache.
  virtual bool IsInBackForwardCache() = 0;

 protected:
  virtual ~WebMessageReplyProxy() = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_REPLY_PROXY_H_
