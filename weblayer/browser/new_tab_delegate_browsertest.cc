// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/public/new_tab_delegate.h"

#include "base/strings/stringprintf.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/browser.h"
#include "weblayer/public/new_tab_delegate.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

class DestroyingNewTabDelegate : public NewTabDelegate {
 public:
  void WaitForOnNewTab() { run_loop_.Run(); }

  bool was_on_new_tab_called() const { return was_on_new_tab_called_; }

  // NewTabDelegate:
  void OnNewTab(Tab* new_tab, NewTabType type) override {
    was_on_new_tab_called_ = true;
    new_tab->GetBrowser()->DestroyTab(new_tab);
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  bool was_on_new_tab_called_ = false;
};

}  // namespace

using NewTabDelegateTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(NewTabDelegateTest, DestroyTabOnNewTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateAndWaitForCompletion(embedded_test_server()->GetURL("/echo"),
                               shell()->tab());
  DestroyingNewTabDelegate new_tab_delegate;
  shell()->tab()->SetNewTabDelegate(&new_tab_delegate);
  GURL popup_url = embedded_test_server()->GetURL("/echo?popup");
  ExecuteScriptWithUserGesture(
      shell()->tab(),
      base::StringPrintf("window.open('%s')", popup_url.spec().c_str()));
  new_tab_delegate.WaitForOnNewTab();
  EXPECT_TRUE(new_tab_delegate.was_on_new_tab_called());
  EXPECT_EQ(1u, shell()->tab()->GetBrowser()->GetTabs().size());
}

}  // namespace weblayer
