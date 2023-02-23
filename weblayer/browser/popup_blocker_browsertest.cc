// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/blocked_content/popup_blocker_tab_helper.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/browser.h"
#include "weblayer/public/browser_observer.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/new_tab_delegate.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/test_navigation_observer.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class PopupBlockerBrowserTest : public WebLayerBrowserTest,
                                public NewTabDelegate,
                                public BrowserObserver {
 public:
  // WebLayerBrowserTest:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    original_tab_ = shell()->tab();
#if !BUILDFLAG(IS_ANDROID)
    // Android does this in Java.
    original_tab_->SetNewTabDelegate(this);
#endif
    shell()->browser()->AddObserver(this);

    NavigateAndWaitForCompletion(embedded_test_server()->GetURL("/echo"),
                                 original_tab_);
  }
  void TearDownOnMainThread() override {
    shell()->browser()->RemoveObserver(this);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebLayerBrowserTest::SetUpCommandLine(command_line);
    // Some bots are flaky due to slower loading interacting with
    // deferred commits.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  // NewTabDelegate:
  void OnNewTab(Tab* new_tab, NewTabType type) override {}

  // BrowserObserver:
  void OnTabAdded(Tab* tab) override {
    new_tab_ = tab;
    if (new_tab_run_loop_)
      new_tab_run_loop_->Quit();
  }
  void OnTabRemoved(Tab* tab, bool active_tab_changed) override {
    ASSERT_EQ(tab, new_tab_);
    new_tab_ = nullptr;
    if (close_tab_run_loop_)
      close_tab_run_loop_->Quit();
  }

  size_t GetBlockedPopupCount() {
    return blocked_content::PopupBlockerTabHelper::FromWebContents(
               GetWebContents(original_tab_))
        ->GetBlockedPopupsCount();
  }

  content::WebContents* GetWebContents(Tab* tab) {
    return static_cast<TabImpl*>(tab)->web_contents();
  }

  Tab* WaitForNewTab() {
    if (!new_tab_) {
      new_tab_run_loop_ = std::make_unique<base::RunLoop>();
      new_tab_run_loop_->Run();
      new_tab_run_loop_ = nullptr;
    }
    return new_tab_;
  }

  void WaitForCloseTab() {
    if (new_tab_) {
      close_tab_run_loop_ = std::make_unique<base::RunLoop>();
      close_tab_run_loop_->Run();
      close_tab_run_loop_ = nullptr;
    }
    ASSERT_FALSE(new_tab_);
  }

  void ExpectTabURL(Tab* tab, const GURL& url) {
    if (tab->GetNavigationController()->GetNavigationListSize() > 0) {
      EXPECT_EQ(tab->GetNavigationController()->GetNavigationEntryDisplayURL(0),
                url);
    } else {
      TestNavigationObserver(
          url, TestNavigationObserver::NavigationEvent::kCompletion, tab)
          .Wait();
    }
  }

  Tab* ShowPopup(const GURL& url) {
    auto* popup_blocker =
        blocked_content::PopupBlockerTabHelper::FromWebContents(
            GetWebContents(original_tab_));
    popup_blocker->ShowBlockedPopup(
        popup_blocker->GetBlockedPopupRequests().begin()->first,
        WindowOpenDisposition::NEW_FOREGROUND_TAB);
    Tab* new_tab = WaitForNewTab();
    ExpectTabURL(new_tab, url);
    EXPECT_EQ(GetBlockedPopupCount(), 0u);
    return new_tab;
  }

  Tab* original_tab() { return original_tab_; }

 private:
  std::unique_ptr<base::RunLoop> new_tab_run_loop_;
  std::unique_ptr<base::RunLoop> close_tab_run_loop_;

  raw_ptr<Tab> original_tab_ = nullptr;
  raw_ptr<Tab> new_tab_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, DISABLED_BlocksPopup) {
  ExecuteScript(original_tab(), "window.open('https://google.com')", true);
  EXPECT_EQ(GetBlockedPopupCount(), 1u);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, DISABLED_BlocksMultiplePopups) {
  ExecuteScript(original_tab(), "window.open('https://google.com')", true);
  ExecuteScript(original_tab(), "window.open('https://google.com')", true);
  EXPECT_EQ(GetBlockedPopupCount(), 2u);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       DISABLED_DoesNotBlockUserGesture) {
  GURL popup_url = embedded_test_server()->GetURL("/echo?popup");
  ExecuteScriptWithUserGesture(
      original_tab(),
      base::StringPrintf("window.open('%s')", popup_url.spec().c_str()));

  Tab* new_tab = WaitForNewTab();
  ExpectTabURL(new_tab, popup_url);
  EXPECT_EQ(GetBlockedPopupCount(), 0u);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest, DISABLED_OpensBlockedPopup) {
  GURL popup_url = embedded_test_server()->GetURL("/echo?popup");
  ExecuteScript(
      original_tab(),
      base::StringPrintf("window.open('%s')", popup_url.spec().c_str()), true);
  EXPECT_EQ(GetBlockedPopupCount(), 1u);

  Tab* new_tab = ShowPopup(popup_url);

  // Blocked popups should no longer have the opener set to match Chrome
  // behavior.
  EXPECT_FALSE(GetWebContents(new_tab)->HasOpener());
  // Make sure we can cleanly close the popup, and there's no crash.
  ExecuteScriptWithUserGesture(new_tab, "window.close()");
  WaitForCloseTab();
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       DISABLED_AllowsPopupThroughContentSettingException) {
  GURL popup_url = embedded_test_server()->GetURL("/echo?popup");
  HostContentSettingsMapFactory::GetForBrowserContext(
      GetWebContents(original_tab())->GetBrowserContext())
      ->SetContentSettingDefaultScope(popup_url, GURL(),
                                      ContentSettingsType::POPUPS,
                                      CONTENT_SETTING_ALLOW);
  ExecuteScript(
      original_tab(),
      base::StringPrintf("window.open('%s')", popup_url.spec().c_str()), true);
  Tab* new_tab = WaitForNewTab();
  ExpectTabURL(new_tab, popup_url);
  EXPECT_EQ(GetBlockedPopupCount(), 0u);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       DISABLED_AllowsPopupThroughContentSettingDefaultValue) {
  GURL popup_url = embedded_test_server()->GetURL("/echo?popup");
  HostContentSettingsMapFactory::GetForBrowserContext(
      GetWebContents(original_tab())->GetBrowserContext())
      ->SetDefaultContentSetting(ContentSettingsType::POPUPS,
                                 CONTENT_SETTING_ALLOW);
  ExecuteScript(
      original_tab(),
      base::StringPrintf("window.open('%s')", popup_url.spec().c_str()), true);
  Tab* new_tab = WaitForNewTab();
  ExpectTabURL(new_tab, popup_url);
  EXPECT_EQ(GetBlockedPopupCount(), 0u);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       DISABLED_BlockPopupThroughContentSettingException) {
  GURL popup_url = embedded_test_server()->GetURL("/echo?popup");
  HostContentSettingsMapFactory::GetForBrowserContext(
      GetWebContents(original_tab())->GetBrowserContext())
      ->SetDefaultContentSetting(ContentSettingsType::POPUPS,
                                 CONTENT_SETTING_ALLOW);
  HostContentSettingsMapFactory::GetForBrowserContext(
      GetWebContents(original_tab())->GetBrowserContext())
      ->SetContentSettingDefaultScope(popup_url, GURL(),
                                      ContentSettingsType::POPUPS,
                                      CONTENT_SETTING_BLOCK);
  ExecuteScript(
      original_tab(),
      base::StringPrintf("window.open('%s')", popup_url.spec().c_str()), true);
  EXPECT_EQ(GetBlockedPopupCount(), 1u);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       DISABLED_BlocksAndOpensPopupFromOpenURL) {
  GURL popup_url = embedded_test_server()->GetURL("/echo?popup");
  content::OpenURLParams params(popup_url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, true);
  params.initiator_origin = url::Origin::Create(popup_url);
  GetWebContents(original_tab())->OpenURL(params);
  EXPECT_EQ(GetBlockedPopupCount(), 1u);

  ShowPopup(popup_url);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       DoesNotOpenPopupWithoutNewTabDelegate) {
  NewTabDelegate* old_delegate =
      static_cast<TabImpl*>(original_tab())->new_tab_delegate();
  original_tab()->SetNewTabDelegate(nullptr);
  GURL popup_url = embedded_test_server()->GetURL("/echo?popup");
  ExecuteScriptWithUserGesture(
      original_tab(),
      base::StringPrintf("window.open('%s')", popup_url.spec().c_str()));
  EXPECT_EQ(GetBlockedPopupCount(), 0u);

  // Navigate the original tab, then make sure we still only have a single tab.
  NavigateAndWaitForCompletion(embedded_test_server()->GetURL("/echo"),
                               original_tab());
  EXPECT_EQ(shell()->browser()->GetTabs().size(), 1u);

  // Restore the old delegate to make sure it is cleaned up on Android.
  original_tab()->SetNewTabDelegate(old_delegate);
}

IN_PROC_BROWSER_TEST_F(PopupBlockerBrowserTest,
                       DoesNotOpenBlockedPopupWithoutNewTabDelegate) {
  NewTabDelegate* old_delegate =
      static_cast<TabImpl*>(original_tab())->new_tab_delegate();
  original_tab()->SetNewTabDelegate(nullptr);
  GURL popup_url = embedded_test_server()->GetURL("/echo?popup");
  ExecuteScript(
      original_tab(),
      base::StringPrintf("window.open('%s')", popup_url.spec().c_str()), true);
  EXPECT_EQ(GetBlockedPopupCount(), 0u);

  // Navigate the original tab, then make sure we still only have a single tab.
  NavigateAndWaitForCompletion(embedded_test_server()->GetURL("/echo"),
                               original_tab());
  EXPECT_EQ(shell()->browser()->GetTabs().size(), 1u);

  // Restore the old delegate to make sure it is cleaned up on Android.
  original_tab()->SetNewTabDelegate(old_delegate);
}

}  // namespace weblayer
