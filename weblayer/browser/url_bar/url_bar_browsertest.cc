// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class UrlBarBrowserTest : public WebLayerBrowserTest {
 public:
  UrlBarBrowserTest() = default;
  ~UrlBarBrowserTest() override = default;

  // WebLayerBrowserTest
  void SetUpOnMainThread() override {
    WebLayerBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    tab_ = static_cast<TabImpl*>(shell()->browser()->CreateTab());
    another_tab_ = static_cast<TabImpl*>(shell()->browser()->CreateTab());
    SetActiveTab(tab_);
  }
  void PostRunTestOnMainThread() override {
    tab_ = nullptr;
    another_tab_ = nullptr;
    WebLayerBrowserTest::PostRunTestOnMainThread();
  }

  GURL real_url() {
    return embedded_test_server()->GetURL("/simple_page.html");
  }
  GURL abort_url() { return embedded_test_server()->GetURL("/nocontent"); }

  void SetVisibleSecurityStateChangedCallback(base::OnceClosure closure) {
    browser_impl()->set_visible_security_state_callback_for_tests(
        std::move(closure));
  }

  void SetActiveTab(TabImpl* tab) { shell()->browser()->SetActiveTab(tab); }

 protected:
  TabImpl* tab_ = nullptr;
  TabImpl* another_tab_ = nullptr;

 private:
  BrowserImpl* browser_impl() {
    return static_cast<BrowserImpl*>(shell()->browser());
  }
};

IN_PROC_BROWSER_TEST_F(UrlBarBrowserTest, CanceledNavigationsUpdateUrl) {
  NavigateAndWaitForCompletion(real_url(), tab_);

  {
    base::RunLoop run_loop;
    SetVisibleSecurityStateChangedCallback(run_loop.QuitClosure());

    // Navigating to the /nocontent url cancels the navigation with a 204 error.
    NavigateAndWaitForStart(abort_url(), tab_);

    // The test won't finish until WebLayer acts on the resulting
    // WebContentsObserver::DidChangeVisibleSecurityState() notification, or the
    // test times out.
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(UrlBarBrowserTest, ChangingActiveTabUpdatesUrlBarView) {
  NavigateAndWaitForCompletion(real_url(), tab_);
  NavigateAndWaitForCompletion(real_url(), another_tab_);

  {
    base::RunLoop run_loop;
    SetVisibleSecurityStateChangedCallback(run_loop.QuitClosure());

    SetActiveTab(another_tab_);

    // The test won't finish until
    // BrowserImpl::VisibleSecurityStateOfActiveTabChanged() gets called.
    run_loop.Run();
  }
}

}  // namespace weblayer
