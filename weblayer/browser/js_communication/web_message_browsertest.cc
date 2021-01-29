// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/public/js_communication/web_message.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "weblayer/public/js_communication/web_message.h"
#include "weblayer/public/js_communication/web_message_host.h"
#include "weblayer/public/js_communication/web_message_host_factory.h"
#include "weblayer/public/js_communication/web_message_reply_proxy.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

class WebMessageHostImpl;
WebMessageHostImpl* current_connection = nullptr;

// WebMessageHost implementation that records contents of OnPostMessage().
class WebMessageHostImpl : public WebMessageHost {
 public:
  WebMessageHostImpl(base::RepeatingClosure quit_closure,
                     const std::string& origin_string,
                     bool is_main_frame,
                     WebMessageReplyProxy* proxy)
      : quit_closure_(quit_closure), proxy_(proxy) {
    current_connection = this;
  }
  ~WebMessageHostImpl() override {
    if (current_connection == this)
      current_connection = nullptr;
  }

  std::vector<base::string16>& messages() { return messages_; }

  // WebMessageHost:
  void OnPostMessage(std::unique_ptr<WebMessage> message) override {
    messages_.push_back(std::move(message->message));
    if (++call_count_ == 1) {
      // First time called, send a message to the page.
      std::unique_ptr<WebMessage> m2 = std::make_unique<WebMessage>();
      m2->message = base::ASCIIToUTF16("from c++");
      proxy_->PostMessage(std::move(m2));
    } else {
      // On subsequent calls quit.
      quit_closure_.Run();
    }
  }

 private:
  int call_count_ = 0;
  base::RepeatingClosure quit_closure_;
  WebMessageReplyProxy* proxy_;
  std::vector<base::string16> messages_;
};

// WebMessageHostFactory implementation that creates WebMessageHostImp.
class WebMessageHostFactoryImpl : public WebMessageHostFactory {
 public:
  explicit WebMessageHostFactoryImpl(base::RepeatingClosure quit_closure)
      : quit_closure_(quit_closure) {}
  ~WebMessageHostFactoryImpl() override = default;

  // WebMessageHostFactory:
  std::unique_ptr<WebMessageHost> CreateHost(
      const std::string& origin_string,
      bool is_main_frame,
      WebMessageReplyProxy* proxy) override {
    return std::make_unique<WebMessageHostImpl>(quit_closure_, origin_string,
                                                is_main_frame, proxy);
  }

 private:
  base::RepeatingClosure quit_closure_;
};

}  // namespace

using WebMessageTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(WebMessageTest, SendAndReceive) {
  EXPECT_TRUE(embedded_test_server()->Start());

  base::RunLoop run_loop;
  shell()->tab()->AddWebMessageHostFactory(
      std::make_unique<WebMessageHostFactoryImpl>(run_loop.QuitClosure()),
      base::ASCIIToUTF16("x"), {"*"});

  // web_message_test.html posts a message immediately.
  shell()->tab()->GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/web_message_test.html"));
  run_loop.Run();

  // There should be two messages. The one from the page, and the ack triggered
  // when WebMessageHostImpl calls PostMessage().
  ASSERT_TRUE(current_connection);
  ASSERT_EQ(2u, current_connection->messages().size());
  EXPECT_EQ(base::ASCIIToUTF16("from page"), current_connection->messages()[0]);
  EXPECT_EQ(base::ASCIIToUTF16("bouncing from c++"),
            current_connection->messages()[1]);
}

}  // namespace weblayer
