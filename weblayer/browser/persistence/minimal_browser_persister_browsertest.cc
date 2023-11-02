// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/persistence/minimal_browser_persister.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/browser_observer.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/test_navigation_observer.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class MinimalBrowserPersisterTest : public WebLayerBrowserTest,
                                    public BrowserObserver {
 public:
  MinimalBrowserPersisterTest() = default;
  ~MinimalBrowserPersisterTest() override = default;

  // WebLayerBrowserTest:
  void SetUpOnMainThread() override {
    WebLayerBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    browser_ = Browser::Create(GetProfile(), nullptr);
    tab_ = static_cast<TabImpl*>(browser_->CreateTab());
    browser_->SetActiveTab(tab_);
  }
  void PostRunTestOnMainThread() override {
    tab_ = nullptr;
    browser_.reset();
    WebLayerBrowserTest::PostRunTestOnMainThread();
  }

  GURL url1() { return embedded_test_server()->GetURL("/simple_page.html"); }

  GURL url2() { return embedded_test_server()->GetURL("/simple_page2.html"); }

  // Persists the current state, then recreates the browser. See
  // BrowserImpl::GetMinimalPersistenceState() for details on
  // |max_size_in_bytes|, 0 means use the default value.
  void RecreateBrowserUsingMinimalState(
      int max_number_of_navigations_per_tab = 0,
      int max_size_in_bytes = 0) {
    std::vector<uint8_t> minimal_state =
        browser_impl()->GetMinimalPersistenceState(
            max_number_of_navigations_per_tab, max_size_in_bytes);
    tab_ = nullptr;
    got_on_tab_added_ = false;
    browser_ = Browser::Create(GetProfile(), nullptr);
    browser_->AddObserver(this);
    RestoreMinimalStateForBrowser(browser_impl(), minimal_state);
    EXPECT_TRUE(got_on_tab_added_);
    browser_->RemoveObserver(this);
    // There is always at least one tab created (even if restore fails).
    ASSERT_GE(browser_->GetTabs().size(), 1u);
    tab_ = static_cast<TabImpl*>(browser_->GetTabs()[0]);
  }

  // BrowserObserver:
  void OnTabAdded(Tab* tab) override {
    got_on_tab_added_ = true;
    EXPECT_TRUE(browser_->IsRestoringPreviousState());
  }

 protected:
  BrowserImpl* browser_impl() {
    return static_cast<BrowserImpl*>(browser_.get());
  }

  bool got_on_tab_added_ = false;
  std::unique_ptr<Browser> browser_;
  raw_ptr<TabImpl> tab_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(MinimalBrowserPersisterTest, SingleTab) {
  NavigateAndWaitForCompletion(url1(), tab_);

  ASSERT_NO_FATAL_FAILURE(RecreateBrowserUsingMinimalState());

  EXPECT_EQ(tab_, browser_->GetActiveTab());
  TestNavigationObserver observer(
      url1(), TestNavigationObserver::NavigationEvent::kCompletion,
      browser_->GetActiveTab());
  observer.Wait();
  EXPECT_EQ(1, tab_->GetNavigationController()->GetNavigationListSize());
}

IN_PROC_BROWSER_TEST_F(MinimalBrowserPersisterTest, TwoTabs) {
  NavigateAndWaitForCompletion(url1(), tab_);

  Tab* tab2 = browser_->CreateTab();
  NavigateAndWaitForCompletion(url2(), tab2);
  browser_->SetActiveTab(tab2);

  // Shutdown the service and run the assertions twice to ensure we handle
  // correctly storing state of tabs that need to be reloaded.
  for (int i = 0; i < 2; ++i) {
    ASSERT_NO_FATAL_FAILURE(RecreateBrowserUsingMinimalState());
    tab2 = nullptr;

    ASSERT_EQ(2u, browser_->GetTabs().size()) << "iteration " << i;
    tab2 = browser_->GetTabs()[1];
    EXPECT_EQ(tab2, browser_->GetActiveTab()) << "iteration " << i;
    // The first tab shouldn't have loaded yet, as it's not active.
    EXPECT_TRUE(tab_->web_contents()->GetController().NeedsReload())
        << "iteration " << i;
    EXPECT_EQ(1, tab2->GetNavigationController()->GetNavigationListSize())
        << "iteration " << i;
    TestNavigationObserver observer(
        url2(), TestNavigationObserver::NavigationEvent::kCompletion, tab2);
  }
}

IN_PROC_BROWSER_TEST_F(MinimalBrowserPersisterTest, PendingSkipped) {
  NavigateAndWaitForCompletion(url1(), tab_);

  tab_->GetNavigationController()->Navigate(url2());

  ASSERT_NO_FATAL_FAILURE(RecreateBrowserUsingMinimalState());

  EXPECT_EQ(tab_, browser_->GetActiveTab());
  TestNavigationObserver observer(
      url1(), TestNavigationObserver::NavigationEvent::kCompletion,
      browser_->GetActiveTab());
  observer.Wait();
  ASSERT_EQ(1, tab_->GetNavigationController()->GetNavigationListSize());
}

IN_PROC_BROWSER_TEST_F(MinimalBrowserPersisterTest, TwoNavs) {
  NavigateAndWaitForCompletion(url1(), tab_);

  NavigateAndWaitForCompletion(url2(), tab_);

  ASSERT_NO_FATAL_FAILURE(RecreateBrowserUsingMinimalState());

  TabImpl* restored_tab = tab_;
  EXPECT_EQ(restored_tab, browser_->GetActiveTab());
  TestNavigationObserver observer(
      url2(), TestNavigationObserver::NavigationEvent::kCompletion,
      restored_tab);
  observer.Wait();
  ASSERT_EQ(2,
            restored_tab->GetNavigationController()->GetNavigationListSize());
  content::NavigationController& nav_controller =
      restored_tab->web_contents()->GetController();
  EXPECT_EQ(1, nav_controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1(), nav_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2(), nav_controller.GetEntryAtIndex(1)->GetURL());
}

IN_PROC_BROWSER_TEST_F(MinimalBrowserPersisterTest, NavigationOverflow) {
  NavigateAndWaitForCompletion(url1(), tab_);
  NavigateAndWaitForCompletion(url2(), tab_);
  const GURL url3 = embedded_test_server()->GetURL("/simple_page3.html");
  NavigateAndWaitForCompletion(url3, tab_);
  const GURL url4 = embedded_test_server()->GetURL("/simple_page4.html");
  NavigateAndWaitForCompletion(url4, tab_);

  ASSERT_NO_FATAL_FAILURE(RecreateBrowserUsingMinimalState(3));

  // As a max of 3 navigations was specified, only the last three navigations
  // should be restored.
  TabImpl* restored_tab = tab_;
  EXPECT_EQ(restored_tab, browser_->GetActiveTab());
  TestNavigationObserver observer(
      url4, TestNavigationObserver::NavigationEvent::kCompletion, restored_tab);
  observer.Wait();
  ASSERT_EQ(3,
            restored_tab->GetNavigationController()->GetNavigationListSize());
  content::NavigationController& nav_controller =
      restored_tab->web_contents()->GetController();
  EXPECT_EQ(2, nav_controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2(), nav_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, nav_controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url4, nav_controller.GetEntryAtIndex(2)->GetURL());
}

// crbug.com/1240904: test is flaky on linux and win.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_Overflow DISABLED_Overflow
#else
#define MAYBE_Overflow Overflow
#endif
IN_PROC_BROWSER_TEST_F(MinimalBrowserPersisterTest, MAYBE_Overflow) {
  std::string url_string(2048, 'a');
  const std::string data = "data:,";
  url_string.replace(0, data.size(), data);
  NavigateAndWaitForCompletion(GURL(url_string), tab_);

  ASSERT_NO_FATAL_FAILURE(RecreateBrowserUsingMinimalState(0, 2048));

  TabImpl* restored_tab = tab_;
  EXPECT_EQ(restored_tab, browser_->GetActiveTab());
  if (blink::features::IsInitialNavigationEntryEnabled()) {
    EXPECT_EQ(1, restored_tab->web_contents()->GetController().GetEntryCount());
  } else {
    EXPECT_EQ(0, restored_tab->web_contents()->GetController().GetEntryCount());
  }
  EXPECT_TRUE(restored_tab->web_contents()->GetController().GetPendingEntry() ==
              nullptr);
}

// Tests that a tab with no committed navigation won't be persisted/restored.
IN_PROC_BROWSER_TEST_F(MinimalBrowserPersisterTest, TabWithNoNavigation) {
  std::vector<uint8_t> minimal_state =
      browser_impl()->GetMinimalPersistenceState();
  got_on_tab_added_ = false;
  RestoreMinimalStateForBrowser(browser_impl(), minimal_state);
  EXPECT_FALSE(got_on_tab_added_);
}

}  // namespace weblayer
