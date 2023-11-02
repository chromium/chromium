// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_REPLY_PROXY_H_
#define WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_REPLY_PROXY_H_

#include <memory>

namespace weblayer {

class Page;
struct WebMessage;

// Used to send messages to the page.
class WebMessageReplyProxy {
 public:
  // To match the JavaScript call, this function would ideally be named
  // PostMessage(), but that conflicts with a Windows macro, so PostWebMessage()
  // is used.
  virtual void PostWebMessage(std::unique_ptr<WebMessage>) = 0;

  // Returns true if the page is in the back/forward cache.
  virtual bool IsInBackForwardCache() = 0;

  // Returns the Page this proxy was created for. This always returns the Page
  // of the main frame.
  virtual Page& GetPage() = 0;

 protected:
  virtual ~WebMessageReplyProxy() = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_JS_COMMUNICATION_WEB_MESSAGE_REPLY_PROXY_H_
