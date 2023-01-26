// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/public/js_communication/web_message.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "net/dns/mock_host_resolver.h"
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

  int back_forward_cache_state_changed_call_count() const {
    return back_forward_cache_state_changed_call_count_;
  }

  void WaitForBackForwardStateToBe(bool value) {
    if (value == proxy_->IsInBackForwardCache())
      return;
    expected_back_forward_value_ = value;
    state_changed_run_loop_ = std::make_unique<base::RunLoop>();
    state_changed_run_loop_->Run();
    state_changed_run_loop_.reset();
  }

  WebMessageReplyProxy* proxy() { return proxy_; }
  std::vector<std::u16string>& messages() { return messages_; }

  // WebMessageHost:
  void OnPostMessage(std::unique_ptr<WebMessage> message) override {
    messages_.push_back(std::move(message->message));
    if (++call_count_ == 1) {
      // First time called, send a message to the page.
      std::unique_ptr<WebMessage> m2 = std::make_unique<WebMessage>();
      m2->message = u"from c++";
      proxy_->PostWebMessage(std::move(m2));
    } else {
      // On subsequent calls quit.
      quit_closure_.Run();
    }
  }
  void OnBackForwardCacheStateChanged() override {
    ++back_forward_cache_state_changed_call_count_;
    if (state_changed_run_loop_ &&
        expected_back_forward_value_ == proxy_->IsInBackForwardCache()) {
      state_changed_run_loop_->Quit();
    }
  }

 private:
  int call_count_ = 0;
  int back_forward_cache_state_changed_call_count_ = 0;
  base::RepeatingClosure quit_closure_;
  raw_ptr<WebMessageReplyProxy> proxy_;
  std::vector<std::u16string> messages_;
  bool expected_back_forward_value_ = false;
  std::unique_ptr<base::RunLoop> state_changed_run_loop_;
};

// WebMessageHostFactory implementation that creates WebMessageHostImpl.
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
      std::make_unique<WebMessageHostFactoryImpl>(run_loop.QuitClosure()), u"x",
      {"*"});

  // web_message_test.html posts a message immediately.
  shell()->tab()->GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/web_message_test.html"));
  run_loop.Run();

  // There should be two messages. The one from the page, and the ack triggered
  // when WebMessageHostImpl calls PostMessage().
  ASSERT_TRUE(current_connection);
  ASSERT_EQ(2u, current_connection->messages().size());
  EXPECT_EQ(u"from page", current_connection->messages()[0]);
  EXPECT_EQ(u"bouncing from c++", current_connection->messages()[1]);
  // WebLayer's Page has no functions, verify it can be requested.
  current_connection->proxy()->GetPage();
}

// Ensures that a listener removed from a post message works.
IN_PROC_BROWSER_TEST_F(WebMessageTest, RemoveFromReceive) {
  EXPECT_TRUE(embedded_test_server()->Start());

  base::RunLoop run_loop;
  shell()->tab()->AddWebMessageHostFactory(
      std::make_unique<WebMessageHostFactoryImpl>(run_loop.QuitClosure()), u"x",
      {"*"});

  // web_message_test.html posts a message immediately.
  shell()->tab()->GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/web_message_test2.html"));
  run_loop.Run();

  // There should be two messages. The one from the page, and the ack triggered
  // when WebMessageHostImpl calls PostMessage().
  ASSERT_TRUE(current_connection);
  ASSERT_EQ(2u, current_connection->messages().size());
  EXPECT_EQ(u"from page", current_connection->messages()[0]);
  EXPECT_EQ(u"bouncing from c++", current_connection->messages()[1]);
  // WebLayer's Page has no functions, verify it can be requested.
  current_connection->proxy()->GetPage();
}

class WebMessageTestWithBfCache : public WebLayerBrowserTest {
 public:
  // WebLayerBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache, {{}}},
         {features::kBackForwardCacheTimeToLiveControl,
          {// Set a very long TTL before expiration (longer than the test
           // timeout) so tests that are expecting deletion don't pass when
           // they shouldn't.
           {"time_to_live_seconds", "3600"}}}},
        // Allow BackForwardCache for all devices regardless of their memory.
        {features::kBackForwardCacheMemoryControls});
    WebLayerBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    WebLayerBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebMessageTestWithBfCache, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1(embedded_test_server()->GetURL("a.com", "/web_message_test.html"));

  base::RunLoop run_loop;
  shell()->tab()->AddWebMessageHostFactory(
      std::make_unique<WebMessageHostFactoryImpl>(run_loop.QuitClosure()), u"x",
      {"http://a.com:" + url1.port()});

  auto* navigation_controller = shell()->tab()->GetNavigationController();
  navigation_controller->Navigate(url1);
  run_loop.Run();

  // A WebMessageHostImpl should be created, and it should not be in the cache.
  WebMessageHostImpl* web_message_host = current_connection;
  ASSERT_TRUE(web_message_host);
  EXPECT_FALSE(web_message_host->proxy()->IsInBackForwardCache());
  EXPECT_EQ(0, web_message_host->back_forward_cache_state_changed_call_count());
  Page* original_page = &(web_message_host->proxy()->GetPage());

  // Navigate to a new host. The old page should go into the cache.
  OneShotNavigationObserver observer1(shell());
  navigation_controller->Navigate(
      embedded_test_server()->GetURL("b.com", "/simple_page.html"));
  observer1.WaitForNavigation();
  EXPECT_TRUE(observer1.completed());
  ASSERT_EQ(current_connection, web_message_host);
  web_message_host->WaitForBackForwardStateToBe(true);
  EXPECT_EQ(1, web_message_host->back_forward_cache_state_changed_call_count());

  // Navigate back.
  OneShotNavigationObserver observer2(shell());
  navigation_controller->GoBack();
  observer2.WaitForNavigation();
  web_message_host->WaitForBackForwardStateToBe(false);
  EXPECT_EQ(2, web_message_host->back_forward_cache_state_changed_call_count());
  EXPECT_EQ(original_page, &(web_message_host->proxy()->GetPage()));
}

}  // namespace weblayer
