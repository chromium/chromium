// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/content/browser/base_blocking_page.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/safe_browsing/real_time_url_lookup_service_factory.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/google_account_access_token_fetch_delegate.h"
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

// Implementation of GoogleAccountAccessTokenFetchDelegate used to exercise safe
// browsing access token fetches.
class TestAccessTokenFetchDelegate
    : public GoogleAccountAccessTokenFetchDelegate {
 public:
  TestAccessTokenFetchDelegate() = default;
  ~TestAccessTokenFetchDelegate() override = default;

  // GoogleAccountAccessTokenFetchDelegate:
  void FetchAccessToken(const std::set<std::string>& scopes,
                        OnTokenFetchedCallback callback) override {
    has_received_request_ = true;
    scopes_from_most_recent_request_ = scopes;

    if (should_respond_to_request_) {
      std::move(callback).Run("token");
    } else {
      outstanding_request_ = std::move(callback);
    }
  }

  void OnAccessTokenIdentifiedAsInvalid(const std::set<std::string>& scopes,
                                        const std::string& token) override {
    NOTREACHED();
  }

  void set_should_respond_to_request(bool should_respond) {
    should_respond_to_request_ = should_respond;
  }

  bool has_received_request() { return has_received_request_; }
  const std::set<std::string>& scopes_from_most_recent_request() {
    return scopes_from_most_recent_request_;
  }

 private:
  bool should_respond_to_request_ = false;
  bool has_received_request_ = false;
  std::set<std::string> scopes_from_most_recent_request_;
  OnTokenFetchedCallback outstanding_request_;
};

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
  raw_ptr<Tab> tab_;
  base::RunLoop run_loop_;
};

using SbBridge = safe_browsing::SafeBrowsingApiHandlerBridge;

void RunCallbackOnIOThread(std::unique_ptr<SbBridge::ResponseCallback> callback,
                           safe_browsing::SBThreatType threat_type,
                           const safe_browsing::ThreatMetadata& metadata) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(*callback), threat_type, metadata));
}

}  // namespace

class TestUrlCheckInterceptor : public safe_browsing::UrlCheckInterceptor {
 public:
  void Add(const GURL& url, safe_browsing::SBThreatType threat_type) {
    map_[url] = threat_type;
  }

  void Clear() { map_.clear(); }

  // safe_browsing::UrlCheckInterceptor
  void Check(std::unique_ptr<SbBridge::ResponseCallback> callback,
             const GURL& url) const override {
    RunCallbackOnIOThread(std::move(callback), Find(url),
                          safe_browsing::ThreatMetadata());
  }
  ~TestUrlCheckInterceptor() override {}

 private:
  safe_browsing::SBThreatType Find(const GURL& url) const {
    auto it = map_.find(url);
    if (it != map_.end())
      return it->second;

    // If the url is not in the map assume it is safe.
    return safe_browsing::SB_THREAT_TYPE_SAFE;
  }

  std::map<GURL, safe_browsing::SBThreatType> map_;
};

class SafeBrowsingBrowserTest : public WebLayerBrowserTest {
 public:
  SafeBrowsingBrowserTest()
      : url_check_interceptor_(std::make_unique<TestUrlCheckInterceptor>()) {}

  SafeBrowsingBrowserTest(const SafeBrowsingBrowserTest&) = delete;
  SafeBrowsingBrowserTest& operator=(const SafeBrowsingBrowserTest&) = delete;

  ~SafeBrowsingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InitializeOnMainThread();
    // Safe Browsing is enabled by default
    ASSERT_TRUE(GetSafeBrowsingEnabled());

    profile()->SetGoogleAccountAccessTokenFetchDelegate(
        &access_token_fetch_delegate_);
  }

  void TearDown() override {
    profile()->SetGoogleAccountAccessTokenFetchDelegate(nullptr);
    SbBridge::GetInstance().SetInterceptorForTesting(nullptr);
  }

  void InitializeOnMainThread() {
    NavigateAndWaitForCompletion(GURL("about:blank"), shell());
    SbBridge::GetInstance().SetInterceptorForTesting(
        url_check_interceptor_.get());

    // Some tests need to be able to navigate to URLs on domains that are not
    // explicitly localhost (e.g., so that realtime URL lookups occur on these
    // navigations).
    host_resolver()->AddRule("*", "127.0.0.1");

    ASSERT_TRUE(embedded_test_server()->Start());
    url_ = embedded_test_server()->GetURL("/simple_page.html");
  }

  void SetSafeBrowsingEnabled(bool value) {
    GetProfile()->SetBooleanSetting(SettingType::BASIC_SAFE_BROWSING_ENABLED,
                                    value);
  }

  void SetRealTimeURLLookupsEnabled(bool value) {
    GetProfile()->SetBooleanSetting(
        SettingType::REAL_TIME_SAFE_BROWSING_ENABLED, value);
  }

  void EnableSafeBrowsingAccessTokenFetches() {
    RealTimeUrlLookupServiceFactory::GetInstance()
        ->set_access_token_fetches_enabled_for_testing();
  }

  bool GetSafeBrowsingEnabled() {
    return GetProfile()->GetBooleanSetting(
        SettingType::BASIC_SAFE_BROWSING_ENABLED);
  }

  void NavigateWithThreatType(const safe_browsing::SBThreatType& threatType,
                              bool expect_interstitial) {
    url_check_interceptor_->Add(url_, threatType);
    Navigate(url_, expect_interstitial);
  }

  void Navigate(const GURL& url, bool expect_interstitial) {
    LoadCompletionObserver load_observer(shell());
    shell()->tab()->GetNavigationController()->Navigate(url);
    load_observer.Wait();
    EXPECT_EQ(expect_interstitial, HasInterstitial());
    if (expect_interstitial) {
      ASSERT_EQ(safe_browsing::SafeBrowsingBlockingPage::kTypeForTesting,
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
    url_check_interceptor_->Add(script_url, threat_type);
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
            ->GetPrimaryMainFrame()
            ->GetProcess();
    content::RenderProcessHostWatcher crash_observer(
        child_process,
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    child_process->Shutdown(0);
    crash_observer.Wait();
  }

  std::unique_ptr<TestUrlCheckInterceptor> url_check_interceptor_;
  GURL url_;

  ProfileImpl* profile() {
    auto* tab_impl = static_cast<TabImpl*>(shell()->tab());
    return tab_impl->profile();
  }

  TestAccessTokenFetchDelegate* access_token_fetch_delegate() {
    return &access_token_fetch_delegate_;
  }

 private:
  TestAccessTokenFetchDelegate access_token_fetch_delegate_;
};

class SafeBrowsingDisabledBrowserTest : public SafeBrowsingBrowserTest {
 public:
  SafeBrowsingDisabledBrowserTest() {}

  SafeBrowsingDisabledBrowserTest(const SafeBrowsingDisabledBrowserTest&) =
      delete;
  SafeBrowsingDisabledBrowserTest& operator=(
      const SafeBrowsingDisabledBrowserTest&) = delete;

  ~SafeBrowsingDisabledBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SetSafeBrowsingEnabled(false);
    SafeBrowsingBrowserTest::InitializeOnMainThread();
    ASSERT_FALSE(GetSafeBrowsingEnabled());
  }
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

    url_check_interceptor_->Clear();
    url_check_interceptor_->Add(url_, threat_type);
    shell()->tab()->GetNavigationController()->Navigate(url_);

    observer.WaitForNavigationFailureWithSafeBrowsingError();
  }
}

// Tests below are disabled due to failures on Android.
// See crbug.com/1340200.
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

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest,
                       NoAccessTokenFetchWhenSafeBrowsingNotEnabled) {
  GURL a_url(embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  NavigateAndWaitForCompletion(a_url, shell()->tab());

  EXPECT_FALSE(access_token_fetch_delegate()->has_received_request());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest,
                       NoAccessTokenFetchInBasicSafeBrowsing) {
  SetSafeBrowsingEnabled(true);

  GURL a_url(embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  NavigateAndWaitForCompletion(a_url, shell()->tab());

  EXPECT_FALSE(access_token_fetch_delegate()->has_received_request());
}

IN_PROC_BROWSER_TEST_F(SafeBrowsingBrowserTest,
                       NoAccessTokenFetchInRealTimeUrlLookupsUnlessEnabled) {
  SetRealTimeURLLookupsEnabled(true);

  GURL a_url(embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  NavigateAndWaitForCompletion(a_url, shell()->tab());

  EXPECT_FALSE(access_token_fetch_delegate()->has_received_request());

  EnableSafeBrowsingAccessTokenFetches();
  access_token_fetch_delegate()->set_should_respond_to_request(true);

  GURL b_url(embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  NavigateAndWaitForCompletion(a_url, shell()->tab());

  std::set<std::string> safe_browsing_scopes = {
      GaiaConstants::kChromeSafeBrowsingOAuth2Scope};
  EXPECT_TRUE(access_token_fetch_delegate()->has_received_request());
  EXPECT_EQ(safe_browsing_scopes,
            access_token_fetch_delegate()->scopes_from_most_recent_request());
}

// Tests that even if the embedder does not respond to an access token fetch
// that is made by safe browsing as part of a navigation, the navigation
// completes due to Safe Browsing's timing out the access token fetch.
IN_PROC_BROWSER_TEST_F(
    SafeBrowsingBrowserTest,
    UnfulfilledAccessTokenFetchTimesOutAndNavigationCompletes) {
  SetRealTimeURLLookupsEnabled(true);
  EnableSafeBrowsingAccessTokenFetches();
  access_token_fetch_delegate()->set_should_respond_to_request(false);

  GURL a_url(embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  NavigateAndWaitForCompletion(a_url, shell()->tab());

  std::set<std::string> safe_browsing_scopes = {
      GaiaConstants::kChromeSafeBrowsingOAuth2Scope};
  EXPECT_TRUE(access_token_fetch_delegate()->has_received_request());
  EXPECT_EQ(safe_browsing_scopes,
            access_token_fetch_delegate()->scopes_from_most_recent_request());
}

}  // namespace weblayer
