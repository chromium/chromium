// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "components/prefs/pref_service.h"
#include "components/safe_browsing/android/safe_browsing_api_handler.h"
#include "components/safe_browsing/content/base_blocking_page.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/navigation_observer.h"
#include "weblayer/public/profile.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/load_completion_observer.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

// Observer customized for safe browsing navigation failures.
class SafeBrowsingErrorNavigationObserver : public NavigationObserver {
 public:
  SafeBrowsingErrorNavigationObserver(const GURL& url, Shell* shell)
      : url_(url), tab_(shell->tab()) {
    tab_->GetNavigationController()->AddObserver(this);
  }

  ~SafeBrowsingErrorNavigationObserver() override {
    tab_->GetNavigationController()->RemoveObserver(this);
  }

  void NavigationFailed(Navigation* navigation) override {
    if (navigation->GetURL() != url_)
      return;

    EXPECT_EQ(navigation->GetLoadError(),
              Navigation::LoadError::kSafeBrowsingError);
    run_loop_.Quit();
  }

  // Begins waiting for a Navigation within |shell_| and to |url_| to fail. In
  // the failure callback verifies that the navigation failed with a safe
  // browsing error.
  void WaitForNavigationFailureWithSafeBrowsingError() { run_loop_.Run(); }

 private:
  const GURL url_;
  Tab* tab_;
  base::RunLoop run_loop_;
};

void RunCallbackOnIOThread(
    std::unique_ptr<safe_browsing::SafeBrowsingApiHandler::URLCheckCallbackMeta>
        callback,
    safe_browsing::SBThreatType threat_type,
    const safe_browsing::ThreatMetadata& metadata) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(*callback), threat_type, metadata));
}

}  // namespace

class FakeSafeBrowsingApiHandler
    : public safe_browsing::SafeBrowsingApiHandler {
 public:
  // SafeBrowsingApiHandler
  void StartURLCheck(
      std::unique_ptr<URLCheckCallbackMeta> callback,
      const GURL& url,
      const safe_browsing::SBThreatTypeSet& threat_types) override {
    RunCallbackOnIOThread(std::move(callback), GetSafeBrowsingRestriction(url),
                          safe_browsing::ThreatMetadata());
  }
  bool StartCSDAllowlistCheck(const GURL& url) override { return false; }
  bool StartHighConfidenceAllowlistCheck(const GURL& url) override {
    return false;
  }

  void AddRestriction(const GURL& url,
                      const safe_browsing::SBThreatType& threat_type) {
    restrictions_[url] = threat_type;
  }

  void ClearRestrictions() { restrictions_.clear(); }

 private:
  safe_browsing::SBThreatType GetSafeBrowsingRestriction(const GURL& url) {
    auto restrictions_iter = restrictions_.find(url);
    if (restrictions_iter == restrictions_.end()) {
      // if the url is not in restrictions assume it's safe.
      return safe_browsing::SB_THREAT_TYPE_SAFE;
    }
    return restrictions_iter->second;
  }

  std::map<GURL, safe_browsing::SBThreatType> restrictions_;
};

class SafeBrowsingBrowserTest : public WebLayerBrowserTest {
 public:
  SafeBrowsingBrowserTest() : fake_handler_(new FakeSafeBrowsingApiHandler()) {}
  ~SafeBrowsingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InitializeOnMainThread();
    // Safe Browsing is enabled by default
    ASSERT_TRUE(GetSafeBrowsingEnabled());
  }

  void InitializeOnMainThread() {
    NavigateAndWaitForCompletion(GURL("about:blank"), shell());
    safe_browsing::SafeBrowsingApiHandler::SetInstance(fake_handler_.get());
    ASSERT_TRUE(embedded_test_server()->Start());
    url_ = embedded_test_server()->GetURL("/simple_page.html");
  }

  void SetSafeBrowsingEnabled(bool value) {
    GetProfile()->SetBooleanSetting(SettingType::BASIC_SAFE_BROWSING_ENABLED,
                                    value);
  }

  bool GetSafeBrowsingEnabled() {
    return GetProfile()->GetBooleanSetting(
        SettingType::BASIC_SAFE_BROWSING_ENABLED);
  }

  void NavigateWithThreatType(const safe_browsing::SBThreatType& threatType,
                              bool expect_interstitial) {
    fake_handler_->AddRestriction(url_, threatType);
    Navigate(url_, expect_interstitial);
  }

  void Navigate(const GURL& url, bool expect_interstitial) {
    LoadCompletionObserver load_observer(shell());
    shell()->tab()->GetNavigationController()->Navigate(url);
    load_observer.Wait();
    EXPECT_EQ(expect_interstitial, HasInterstitial());
    if (expect_interstitial) {
      ASSERT_EQ(SafeBrowsingBlockingPage::kTypeForTesting,
                GetSecurityInterstitialPage()->GetTypeForTesting());
      EXPECT_TRUE(GetSecurityInterstitialPage()->GetHTMLContents().length() >
                  0);
    }
  }

  void NavigateWithSubResourceAndThreatType(
      const safe_browsing::SBThreatType& threat_type,
      bool expect_interstitial) {
    GURL page_with_script_url =
        embedded_test_server()->GetURL("/simple_page_with_script.html");
    GURL script_url = embedded_test_server()->GetURL("/script.js");
    fake_handler_->AddRestriction(script_url, threat_type);
    Navigate(page_with_script_url, expect_interstitial);
  }

 protected:
  content::WebContents* GetWebContents() {
    Tab* tab = shell()->tab();
    TabImpl* tab_impl = static_cast<TabImpl*>(tab);
    return tab_impl->web_contents();
  }

  security_interstitials::SecurityInterstitialPage*
  GetSecurityInterstitialPage() {
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            GetWebContents());
    return helper
               ? helper
                     ->GetBlockingPageForCurrentlyCommittedNavigationForTesting()
               : nullptr;
  }

  bool HasInterstitial() { return GetSecurityInterstitialPage() != nullptr; }

  void KillRenderer() {
    content::RenderProcessHost* child_process =
        static_cast<TabImpl*>(shell()->tab())
            ->web_contents()
            ->GetMainFrame()
            ->GetProcess();
    content::RenderProcessHostWatcher crash_observer(
        child_process,
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    child_process->Shutdown(0);
    crash_observer.Wait();
  }

  std::unique_ptr<FakeSafeBrowsingApiHandler> fake_handler_;
  GURL url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBrowserTest);
};

class SafeBrowsingDisabledBrowserTest : public SafeBrowsingBrowserTest {
 public:
  SafeBrowsingDisabledBrowserTest() {}
  ~SafeBrowsingDisabledBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SetSafeBrowsingEnabled(false);
    SafeBrowsingBrowserTest::InitializeOnMainThread();
    ASSERT_FALSE(GetSafeBrowsingEnabled());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingDisabledBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest,
                       DoesNotShowInterstitial_NoRestriction) {
  Navigate(url_, false);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest, DoesNotShowInterstitial_Safe) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_SAFE, false);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest, ShowsInterstitial_Malware) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_URL_MALWARE, true);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest, ShowsInterstitial_Phishing) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_URL_PHISHING, true);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest, CheckNavigationErrorType) {
  auto threat_types = {
      safe_browsing::SB_THREAT_TYPE_URL_PHISHING,
      safe_browsing::SB_THREAT_TYPE_URL_MALWARE,
      safe_browsing::SB_THREAT_TYPE_URL_UNWANTED,
      safe_browsing::SB_THREAT_TYPE_BILLING,
  };

  for (auto threat_type : threat_types) {
    SafeBrowsingErrorNavigationObserver observer(url_, shell());

    fake_handler_->ClearRestrictions();
    fake_handler_->AddRestriction(url_, threat_type);
    shell()->tab()->GetNavigationController()->Navigate(url_);

    observer.WaitForNavigationFailureWithSafeBrowsingError();
  }
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest, ShowsInterstitial_Unwanted) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_URL_UNWANTED, true);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest, ShowsInterstitial_Billing) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_BILLING, true);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest,
                       ShowsInterstitial_Malware_Subresource) {
  NavigateWithSubResourceAndThreatType(
      safe_browsing::SB_THREAT_TYPE_URL_MALWARE, true);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest,
                       DoesNotShowInterstitial_Phishing_disableSB) {
  // Test that the browser checks the safe browsing setting for new navigations.
  SetSafeBrowsingEnabled(false);
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_URL_PHISHING, false);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest,
                       DoesNotShowInterstitial_Malware_Subresource_disableSB) {
  // Test that new renderer checks the safe browsing setting.
  SetSafeBrowsingEnabled(false);
  KillRenderer();
  NavigateWithSubResourceAndThreatType(
      safe_browsing::SB_THREAT_TYPE_URL_MALWARE, false);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest, CheckSetsPrefs) {
  // Check that changing safe browsing setting sets corresponding pref,
  // which is persistent.
  PrefService* prefs = GetProfile()->GetBrowserContext()->pref_service();
  SetSafeBrowsingEnabled(true);
  EXPECT_TRUE(prefs->GetBoolean(::prefs::kSafeBrowsingEnabled));
  SetSafeBrowsingEnabled(false);
  EXPECT_FALSE(prefs->GetBoolean(::prefs::kSafeBrowsingEnabled));
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingDisabledBrowserTest,
                       DoesNotShowInterstitial_NoRestriction) {
  Navigate(url_, false);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingDisabledBrowserTest,
                       DoesNotShowInterstitial_Safe) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_SAFE, false);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingDisabledBrowserTest,
                       DoesNotShowInterstitial_Malware) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_URL_MALWARE, false);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingDisabledBrowserTest,
                       DoesNotShowInterstitial_Phishing) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_URL_PHISHING, false);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingDisabledBrowserTest,
                       DoesNotShowInterstitial_Unwanted) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_URL_UNWANTED, false);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingDisabledBrowserTest,
                       DoesNotShowInterstitial_Billing) {
  NavigateWithThreatType(safe_browsing::SB_THREAT_TYPE_BILLING, false);
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingDisabledBrowserTest,
                       DoesNotShowInterstitial_Malware_Subresource) {
  NavigateWithSubResourceAndThreatType(
      safe_browsing::SB_THREAT_TYPE_URL_MALWARE, false);
}

}  // namespace weblayer
