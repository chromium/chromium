// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/network_time/network_time_tracker.h"
#include "components/security_interstitials/content/insecure_form_blocking_page.h"
#include "components/security_interstitials/content/ssl_error_assistant.h"
#include "components/security_interstitials/content/ssl_error_handler.h"
#include "net/ssl/ssl_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/weblayer_security_blocking_page_factory.h"
#include "weblayer/public/browser.h"
#include "weblayer/public/browser_observer.h"
#include "weblayer/public/error_page.h"
#include "weblayer/public/error_page_delegate.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/interstitial_utils.h"
#include "weblayer/test/load_completion_observer.h"
#include "weblayer/test/test_navigation_observer.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {
namespace {

#if BUILDFLAG(IS_ANDROID)
// Waits for a new tab to be created, and then load |url|.
class NewTabWaiter : public BrowserObserver {
 public:
  NewTabWaiter(Browser* browser, const GURL& url) : url_(url) {
    observation_.Observe(browser);
  }

  void OnTabAdded(Tab* tab) override {
    navigation_observer_ = std::make_unique<TestNavigationObserver>(
        url_, TestNavigationObserver::NavigationEvent::kStart, tab);
    run_loop_.Quit();
  }

  void Wait() {
    if (!navigation_observer_)
      run_loop_.Run();
    navigation_observer_->Wait();
  }

 private:
  GURL url_;
  std::unique_ptr<TestNavigationObserver> navigation_observer_;
  base::RunLoop run_loop_;
  base::ScopedObservation<Browser, BrowserObserver> observation_{this};
};
#endif

class TestErrorPageDelegate : public ErrorPageDelegate {
 public:
  bool was_get_error_page_content_called() const {
    return was_get_error_page_content_called_;
  }

  // ErrorPageDelegate:
  bool OnBackToSafety() override { return false; }
  std::unique_ptr<ErrorPage> GetErrorPageContent(
      Navigation* navigation) override {
    was_get_error_page_content_called_ = true;
    return std::make_unique<ErrorPage>();
  }

 private:
  bool was_get_error_page_content_called_ = false;
};

}  // namespace

class SSLBrowserTest : public WebLayerBrowserTest {
 public:
  SSLBrowserTest() = default;

  SSLBrowserTest(const SSLBrowserTest&) = delete;
  SSLBrowserTest& operator=(const SSLBrowserTest&) = delete;

  ~SSLBrowserTest() override = default;

  // WebLayerBrowserTest:
  void SetUpOnMainThread() override {
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));

    https_server_mismatched_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_mismatched_->SetSSLConfig(
        net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    https_server_mismatched_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));

    https_server_expired_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_expired_->SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
    https_server_expired_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));

    ASSERT_TRUE(https_server_->Start());
    ASSERT_TRUE(https_server_mismatched_->Start());
    ASSERT_TRUE(https_server_expired_->Start());
  }

  void PostRunTestOnMainThread() override {
    https_server_.reset();
    https_server_mismatched_.reset();
    WebLayerBrowserTest::PostRunTestOnMainThread();
  }

  void NavigateToOkPage() {
    ASSERT_EQ("127.0.0.1", ok_url().host());
    NavigateAndWaitForCompletion(ok_url(), shell());
    EXPECT_FALSE(IsShowingSecurityInterstitial(shell()->tab()));
  }

  void NavigateToPageWithMismatchedCertExpectSSLInterstitial() {
    // Do a navigation that should result in an SSL error.
    NavigateAndWaitForFailure(mismatched_cert_url(), shell());
    // First check that there *is* an interstitial.
    ASSERT_TRUE(IsShowingSecurityInterstitial(shell()->tab()));

    // Now verify that the interstitial is in fact an SSL interstitial.
    EXPECT_TRUE(IsShowingSSLInterstitial(shell()->tab()));

    // TODO(blundell): Check the security state once security state is available
    // via the public WebLayer API, following the example of //chrome's
    // ssl_browsertest.cc's CheckAuthenticationBrokenState() function.
  }

  void NavigateToPageWithMismatchedCertExpectCaptivePortalInterstitial() {
    // Do a navigation that should result in an SSL error.
    NavigateAndWaitForFailure(mismatched_cert_url(), shell());
    // First check that there *is* an interstitial.
    ASSERT_TRUE(IsShowingSecurityInterstitial(shell()->tab()));

    // Now verify that the interstitial is in fact a captive portal
    // interstitial.
    EXPECT_TRUE(IsShowingCaptivePortalInterstitial(shell()->tab()));

    // TODO(blundell): Check the security state once security state is available
    // via the public WebLayer API, following the example of //chrome's
    // ssl_browsertest.cc's CheckAuthenticationBrokenState() function.
  }

  void NavigateToPageWithExpiredCertExpectSSLInterstitial() {
    // Do a navigation that should result in an SSL error.
    NavigateAndWaitForFailure(expired_cert_url(), shell());
    // First check that there *is* an interstitial.
    ASSERT_TRUE(IsShowingSecurityInterstitial(shell()->tab()));

    // Now verify that the interstitial is in fact an SSL interstitial.
    EXPECT_TRUE(IsShowingSSLInterstitial(shell()->tab()));

    // TODO(blundell): Check the security state once security state is available
    // via the public WebLayer API, following the example of //chrome's
    // ssl_browsertest.cc's CheckAuthenticationBrokenState() function.
  }

  void NavigateToPageWithExpiredCertExpectBadClockInterstitial() {
    // Do a navigation that should result in an SSL error.
    NavigateAndWaitForFailure(expired_cert_url(), shell());
    // First check that there *is* an interstitial.
    ASSERT_TRUE(IsShowingSecurityInterstitial(shell()->tab()));

    // Now verify that the interstitial is in fact a bad clock interstitial.
    EXPECT_TRUE(IsShowingBadClockInterstitial(shell()->tab()));

    // TODO(blundell): Check the security state once security state is available
    // via the public WebLayer API, following the example of //chrome's
    // ssl_browsertest.cc's CheckAuthenticationBrokenState() function.
  }

  void NavigateToPageWithMismatchedCertExpectNotBlocked() {
    NavigateAndWaitForCompletion(mismatched_cert_url(), shell());
    EXPECT_FALSE(IsShowingSecurityInterstitial(shell()->tab()));

    // TODO(blundell): Check the security state once security state is available
    // via the public WebLayer API, following the example of //chrome's
    // ssl_browsertest.cc's CheckAuthenticationBrokenState() function.
  }

  void SendInterstitialNavigationCommandAndWait(
      bool proceed,
      absl::optional<GURL> previous_url = absl::nullopt) {
    GURL expected_url =
        proceed ? mismatched_cert_url() : previous_url.value_or(ok_url());
    ASSERT_TRUE(IsShowingSSLInterstitial(shell()->tab()));

    TestNavigationObserver navigation_observer(
        expected_url, TestNavigationObserver::NavigationEvent::kCompletion,
        shell());
    ExecuteScript(shell(),
                  "window.certificateErrorPageController." +
                      std::string(proceed ? "proceed" : "dontProceed") + "();",
                  false /*use_separate_isolate*/);
    navigation_observer.Wait();
    EXPECT_FALSE(IsShowingSSLInterstitial(shell()->tab()));
  }

  void SendInterstitialReloadCommandAndWait() {
    ASSERT_TRUE(IsShowingSSLInterstitial(shell()->tab()));

    LoadCompletionObserver load_observer(shell());
    ExecuteScript(shell(), "window.certificateErrorPageController.reload();",
                  false /*use_separate_isolate*/);
    load_observer.Wait();

    // Should still be showing the SSL interstitial after the reload command is
    // processed.
    EXPECT_TRUE(IsShowingSSLInterstitial(shell()->tab()));
  }

#if BUILDFLAG(IS_ANDROID)
  void SendInterstitialOpenLoginCommandAndWait() {
    ASSERT_TRUE(IsShowingCaptivePortalInterstitial(shell()->tab()));

    // Note: The embedded test server cannot actually load the captive portal
    // login URL, so simply detect the start of the navigation to the page.
    NewTabWaiter waiter(shell()->browser(),
                        WebLayerSecurityBlockingPageFactory::
                            GetCaptivePortalLoginPageUrlForTesting());
    ExecuteScript(shell(), "window.certificateErrorPageController.openLogin();",
                  false /*use_separate_isolate*/);
    waiter.Wait();
  }
#endif

  void NavigateToOtherOkPage() {
    NavigateAndWaitForCompletion(https_server_->GetURL("/simple_page2.html"),
                                 shell());
    EXPECT_FALSE(IsShowingSecurityInterstitial(shell()->tab()));
  }

  GURL ok_url() { return https_server_->GetURL("/simple_page.html"); }
  GURL mismatched_cert_url() {
    return https_server_mismatched_->GetURL("/simple_page.html");
  }

  GURL expired_cert_url() {
    return https_server_expired_->GetURL("/simple_page.html");
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_mismatched_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_expired_;
};

// Tests clicking "take me back" on the interstitial page.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, TakeMeBack) {
  NavigateToOkPage();
  NavigateToPageWithMismatchedCertExpectSSLInterstitial();

  // Click "Take me back".
  SendInterstitialNavigationCommandAndWait(false /*proceed*/);

  // Check that it's possible to navigate to a new page.
  NavigateToOtherOkPage();

  // Navigate to the bad SSL page again, an interstitial shows again (in
  // contrast to what would happen had the user chosen to proceed).
  NavigateToPageWithMismatchedCertExpectSSLInterstitial();
}

// Tests clicking "take me back" on the interstitial page when there's no
// navigation history. The user should be taken to a safe page (about:blank).
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, TakeMeBackEmptyNavigationHistory) {
  NavigateToPageWithMismatchedCertExpectSSLInterstitial();

  // Click "Take me back".
  SendInterstitialNavigationCommandAndWait(false /*proceed*/,
                                           GURL("about:blank"));
}

IN_PROC_BROWSER_TEST_F(SSLBrowserTest, Reload) {
  NavigateToOkPage();
  NavigateToPageWithMismatchedCertExpectSSLInterstitial();

  SendInterstitialReloadCommandAndWait();

  // TODO(blundell): Ideally we would fix the SSL error, reload, and verify
  // that the SSL interstitial isn't showing. However, currently this doesn't
  // work: Calling ResetSSLConfig() on |http_server_mismatched_| passing
  // CERT_OK does not cause future reloads or navigations to
  // mismatched_cert_url() to succeed; they still fail and pop an interstitial.
  // I verified that the LoadCompletionObserver is in fact waiting for a new
  // load, i.e., there is actually a *new* SSL interstitial popped up. From
  // looking at the ResetSSLConfig() impl there shouldn't be any waiting or
  // anything needed within the client.
}

// Tests clicking proceed link on the interstitial page. This is a PRE_ test
// because it also acts as setup for the test below which verifies the behavior
// across restarts.
// TODO(crbug.com/654704): Android does not support PRE_ tests. For Android just
// run only the PRE_ version of this test.
#if BUILDFLAG(IS_ANDROID)
#define PRE_Proceed Proceed
#endif
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, PRE_Proceed) {
  NavigateToOkPage();
  NavigateToPageWithMismatchedCertExpectSSLInterstitial();
  SendInterstitialNavigationCommandAndWait(true /*proceed*/);

  // Go back to an OK page, then try to navigate again. The "Proceed" decision
  // should be saved, so no interstitial is shown this time.
  NavigateToOkPage();
  NavigateToPageWithMismatchedCertExpectNotBlocked();
}

#if !BUILDFLAG(IS_ANDROID)
// The proceed decision is perpetuated across WebLayer sessions, i.e.  WebLayer
// will not block again when navigating to the same bad page that was previously
// proceeded through.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, Proceed) {
  NavigateToPageWithMismatchedCertExpectNotBlocked();
}
#endif

// Tests navigating away from the interstitial page.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, NavigateAway) {
  NavigateToOkPage();
  NavigateToPageWithMismatchedCertExpectSSLInterstitial();
  NavigateToOtherOkPage();
}

// Tests the scenario where the OS reports that an SSL error is due to a
// captive portal. A captive portal interstitial should be displayed. The test
// then switches OS captive portal status to false and reloads the page. This
// time, a normal SSL interstitial should be displayed.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, OSReportsCaptivePortal) {
  SSLErrorHandler::SetOSReportsCaptivePortalForTesting(true);

  NavigateToPageWithMismatchedCertExpectCaptivePortalInterstitial();

  // Check that clearing the test setting causes behavior to revert to normal.
  SSLErrorHandler::SetOSReportsCaptivePortalForTesting(false);
  NavigateToPageWithMismatchedCertExpectSSLInterstitial();
}

#if BUILDFLAG(IS_ANDROID)
// Tests that after reaching a captive portal interstitial, clicking on the
// connect link will cause a navigation to the login page.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest,
                       DISABLED_CaptivePortalConnectToLoginPage) {
  SSLErrorHandler::SetOSReportsCaptivePortalForTesting(true);

  NavigateToPageWithMismatchedCertExpectCaptivePortalInterstitial();

  SendInterstitialOpenLoginCommandAndWait();
}
#endif

IN_PROC_BROWSER_TEST_F(SSLBrowserTest, BadClockInterstitial) {
  // Without the NetworkTimeTracker reporting that the clock is ahead or
  // behind, navigating to a page with an expired cert should result in the
  // default SSL interstitial appearing.
  NavigateToPageWithExpiredCertExpectSSLInterstitial();

  // Set network time back ten minutes.
  BrowserProcess::GetInstance()->GetNetworkTimeTracker()->UpdateNetworkTime(
      base::Time::Now() - base::Minutes(10),
      base::Milliseconds(1),   /* resolution */
      base::Milliseconds(500), /* latency */
      base::TimeTicks::Now() /* posting time of this update */);

  // Now navigating to a page with an expired cert should cause the bad clock
  // interstitial to appear.
  NavigateToPageWithExpiredCertExpectBadClockInterstitial();
}

// This test verifies that a certificate in the list of known captive portal
// certificates in ssl_error_assistant.asciipb is detected as such. This serves
// to verify that the ssl_error_assistant proto was correctly loaded.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest,
                       CertificateInKnownCaptivePortalsListDetected) {
  net::SSLInfo ssl_info_with_known_captive_portal_cert;
  net::HashValue captive_portal_public_key;

  // Set up the SSSLInfo with the certificate of captive-portal.badssl.com
  // (taken from ssl_error_assistant.asciipb).
  ASSERT_TRUE(captive_portal_public_key.FromString(
      "sha256/fjZPHewEHTrMDX3I1ecEIeoy3WFxHyGplOLv28kIbtI="));
  net::HashValueVector public_keys;
  public_keys.push_back(captive_portal_public_key);
  ssl_info_with_known_captive_portal_cert.public_key_hashes = public_keys;

  EXPECT_TRUE(SSLErrorAssistant().IsKnownCaptivePortalCertificate(
      ssl_info_with_known_captive_portal_cert));
}

// Verifies an error page is not requested for an ssl error.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, ErrorPageNotCalledForMismatch) {
  TestErrorPageDelegate error_page_delegate;
  shell()->tab()->SetErrorPageDelegate(&error_page_delegate);
  NavigateToOkPage();
  EXPECT_FALSE(error_page_delegate.was_get_error_page_content_called());
  NavigateToPageWithMismatchedCertExpectSSLInterstitial();
  EXPECT_FALSE(error_page_delegate.was_get_error_page_content_called());
}

// Visits a page that displays an insecure form, submits the form, and checks an
// interstitial is shown.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest,
                       TestDisplaysInsecureFormSubmissionWarning) {
  GURL insecure_form_url = https_server_->GetURL("/insecure_form.html");
  GURL form_target_url = GURL("http://does-not-exist.test/form_target.html?");
  NavigateAndWaitForCompletion(insecure_form_url, shell());

  // Submit the form and wait for the interstitial to load.
  TestNavigationObserver navigation_observer(
      form_target_url, TestNavigationObserver::NavigationEvent::kFailure,
      shell());
  ExecuteScript(shell(), "submitForm();", false /*use_separate_isolate*/);
  navigation_observer.Wait();

  // Check the correct interstitial loaded.
  EXPECT_TRUE(IsShowingInsecureFormInterstitial(shell()->tab()));
}

}  // namespace weblayer
