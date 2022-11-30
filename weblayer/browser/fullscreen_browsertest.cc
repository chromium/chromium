// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/callback.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/browser.h"
#include "weblayer/public/fullscreen_delegate.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

using FullscreenBrowserTest = WebLayerBrowserTest;

class FullscreenDelegateImpl : public FullscreenDelegate {
 public:
  bool got_enter() const { return got_enter_; }
  bool got_exit() const { return got_exit_; }

  void ResetState() { got_enter_ = got_exit_ = false; }

  // FullscreenDelegate:
  void EnterFullscreen(base::OnceClosure exit_closure) override {
    got_enter_ = true;
  }
  void ExitFullscreen() override { got_exit_ = true; }

 private:
  bool got_enter_ = false;
  bool got_exit_ = false;
};

IN_PROC_BROWSER_TEST_F(FullscreenBrowserTest, EnterFromBackgroundTab) {
  EXPECT_TRUE(embedded_test_server()->Start());

  TabImpl* tab1 = static_cast<TabImpl*>(shell()->tab());
  Browser* browser = tab1->GetBrowser();
  TabImpl* tab2 = static_cast<TabImpl*>(browser->CreateTab());
  EXPECT_NE(tab2, browser->GetActiveTab());
  FullscreenDelegateImpl fullscreen_delegate;
  tab2->SetFullscreenDelegate(&fullscreen_delegate);

  // As `tab2` is in the background, the delegate should not be notified.
  static_cast<content::WebContentsDelegate*>(tab2)->EnterFullscreenModeForTab(
      nullptr, blink::mojom::FullscreenOptions());
  EXPECT_TRUE(static_cast<content::WebContentsDelegate*>(tab2)
                  ->IsFullscreenForTabOrPending(nullptr));
  EXPECT_FALSE(fullscreen_delegate.got_enter());

  // Making the tab active should trigger the going fullscreen.
  browser->SetActiveTab(tab2);
  EXPECT_TRUE(static_cast<content::WebContentsDelegate*>(tab2)
                  ->IsFullscreenForTabOrPending(nullptr));
  EXPECT_TRUE(fullscreen_delegate.got_enter());

  tab2->SetFullscreenDelegate(nullptr);
}

IN_PROC_BROWSER_TEST_F(FullscreenBrowserTest, NoExitForBackgroundTab) {
  EXPECT_TRUE(embedded_test_server()->Start());

  Browser* browser = shell()->tab()->GetBrowser();
  TabImpl* tab = static_cast<TabImpl*>(browser->CreateTab());
  EXPECT_NE(tab, browser->GetActiveTab());
  FullscreenDelegateImpl fullscreen_delegate;
  tab->SetFullscreenDelegate(&fullscreen_delegate);

  // As `tab` is in the background, the delegate should not be notified.
  static_cast<content::WebContentsDelegate*>(tab)->EnterFullscreenModeForTab(
      nullptr, blink::mojom::FullscreenOptions());
  EXPECT_TRUE(static_cast<content::WebContentsDelegate*>(tab)
                  ->IsFullscreenForTabOrPending(nullptr));
  EXPECT_FALSE(fullscreen_delegate.got_enter());
  EXPECT_TRUE(static_cast<content::WebContentsDelegate*>(tab)
                  ->IsFullscreenForTabOrPending(nullptr));
  EXPECT_FALSE(fullscreen_delegate.got_enter());
  EXPECT_FALSE(fullscreen_delegate.got_exit());
  fullscreen_delegate.ResetState();

  // Simulate exiting. As the delegate wasn't told about the enter, it should
  // not be told about the exit.
  static_cast<content::WebContentsDelegate*>(tab)->ExitFullscreenModeForTab(
      nullptr);
  EXPECT_FALSE(static_cast<content::WebContentsDelegate*>(tab)
                   ->IsFullscreenForTabOrPending(nullptr));
  EXPECT_FALSE(fullscreen_delegate.got_enter());
  EXPECT_FALSE(fullscreen_delegate.got_exit());

  tab->SetFullscreenDelegate(nullptr);
}

IN_PROC_BROWSER_TEST_F(FullscreenBrowserTest, DelegateNotCalledMoreThanOnce) {
  EXPECT_TRUE(embedded_test_server()->Start());

  // The tab needs to be made active as fullscreen requests for inactive tabs
  // are ignored.
  TabImpl* tab = static_cast<TabImpl*>(shell()->tab());
  tab->GetBrowser()->SetActiveTab(tab);

  FullscreenDelegateImpl fullscreen_delegate;
  tab->SetFullscreenDelegate(&fullscreen_delegate);
  static_cast<content::WebContentsDelegate*>(tab)->EnterFullscreenModeForTab(
      nullptr, blink::mojom::FullscreenOptions());
  EXPECT_TRUE(static_cast<content::WebContentsDelegate*>(tab)
                  ->IsFullscreenForTabOrPending(nullptr));
  EXPECT_TRUE(fullscreen_delegate.got_enter());
  EXPECT_FALSE(fullscreen_delegate.got_exit());
  fullscreen_delegate.ResetState();

  // Simulate another enter. As the tab is already fullscreen the delegate
  // should not be notified again.
  static_cast<content::WebContentsDelegate*>(tab)->EnterFullscreenModeForTab(
      nullptr, blink::mojom::FullscreenOptions());
  EXPECT_TRUE(static_cast<content::WebContentsDelegate*>(tab)
                  ->IsFullscreenForTabOrPending(nullptr));
  EXPECT_FALSE(fullscreen_delegate.got_enter());
  EXPECT_FALSE(fullscreen_delegate.got_exit());

  tab->SetFullscreenDelegate(nullptr);
}

}  // namespace weblayer
