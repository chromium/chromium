// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/browser_controls_navigation_state_handler.h"
#include "weblayer/browser/browser_controls_navigation_state_handler_delegate.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class BrowserConrolsNavigationStateHandlerBrowserTest
    : public WebLayerBrowserTest {
 public:
  BrowserConrolsNavigationStateHandlerBrowserTest() = default;
  ~BrowserConrolsNavigationStateHandlerBrowserTest() override = default;

  // WebLayerBrowserTest:
  void SetUpOnMainThread() override {
    WebLayerBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* web_contents() {
    return static_cast<TabImpl*>(shell()->tab())->web_contents();
  }

 private:
};

class TestBrowserControlsNavigationStateHandlerDelegate
    : public BrowserControlsNavigationStateHandlerDelegate {
 public:
  // BrowserControlsNavigationStateHandlerDelegate:
  void OnBrowserControlsStateStateChanged(
      ControlsVisibilityReason reason,
      cc::BrowserControlsState state) override {
    state_ = state;
    if (quit_callback_)
      std::move(quit_callback_).Run();
  }
  void OnUpdateBrowserControlsStateBecauseOfProcessSwitch(
      bool did_commit) override {}

  void WaitForStateChanged() {
    base::RunLoop run_loop;
    quit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  cc::BrowserControlsState state() { return state_; }

 private:
  base::OnceClosure quit_callback_;
  cc::BrowserControlsState state_ = cc::BrowserControlsState::kBoth;
};

// Tests that BrowserConrolsNavigationStateHandler informs that the status is
// updated according to navigation progress.
IN_PROC_BROWSER_TEST_F(BrowserConrolsNavigationStateHandlerBrowserTest, Basic) {
  TestBrowserControlsNavigationStateHandlerDelegate test_delegate;
  BrowserControlsNavigationStateHandler
      browser_controls_navigation_state_handler(web_contents(), &test_delegate);
  GURL test_url(embedded_test_server()->GetURL("/simple_page.html"));
  NavigateAndWaitForStart(test_url, shell()->tab());
  // `test_delegate` should get the status is updated to `kShown` on
  // DidStartNavigation();
  EXPECT_EQ(test_delegate.state(), cc::BrowserControlsState::kShown);
  test_delegate.WaitForStateChanged();
  // `test_delegate` should get the status is updated to `kBoth` on
  // DidFinishLoad();
  EXPECT_EQ(web_contents()->GetLastCommittedURL(), test_url);
  EXPECT_EQ(test_delegate.state(), cc::BrowserControlsState::kBoth);
}

}  // namespace weblayer
