// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/interstitial_utils.h"
#include "weblayer/test/load_completion_observer.h"
#include "weblayer/test/test_navigation_observer.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class SSLBrowserTest : public WebLayerBrowserTest {
 public:
  SSLBrowserTest() = default;
  ~SSLBrowserTest() override = default;

  // WebLayerBrowserTest:
  void PreRunTestOnMainThread() override {
    WebLayerBrowserTest::PreRunTestOnMainThread();

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

    ASSERT_TRUE(https_server_->Start());
    ASSERT_TRUE(https_server_mismatched_->Start());
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

  void NavigateToPageWithSslErrorExpectBlocked() {
    // Do a navigation that should result in an SSL error.
    NavigateAndWaitForFailure(bad_ssl_url(), shell());
    // First check that there *is* an interstitial.
    ASSERT_TRUE(IsShowingSecurityInterstitial(shell()->tab()));

    // Now verify that the interstitial is in fact an SSL interstitial.
    EXPECT_TRUE(IsShowingSSLInterstitial(shell()->tab()));

    // TODO(blundell): Check the security state once security state is available
    // via the public WebLayer API, following the example of //chrome's
    // ssl_browsertest.cc's CheckAuthenticationBrokenState() function.
  }

  void NavigateToPageWithSslErrorExpectNotBlocked() {
    NavigateAndWaitForCompletion(bad_ssl_url(), shell());
    EXPECT_FALSE(IsShowingSecurityInterstitial(shell()->tab()));

    // TODO(blundell): Check the security state once security state is available
    // via the public WebLayer API, following the example of //chrome's
    // ssl_browsertest.cc's CheckAuthenticationBrokenState() function.
  }

  void SendInterstitialNavigationCommandAndWait(
      bool proceed,
      base::Optional<GURL> previous_url = base::nullopt) {
    GURL expected_url =
        proceed ? bad_ssl_url() : previous_url.value_or(ok_url());
    ASSERT_TRUE(IsShowingSSLInterstitial(shell()->tab()));

    TestNavigationObserver navigation_observer(
        expected_url, TestNavigationObserver::NavigationEvent::Completion,
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

  void NavigateToOtherOkPage() {
    NavigateAndWaitForCompletion(https_server_->GetURL("/simple_page2.html"),
                                 shell());
    EXPECT_FALSE(IsShowingSecurityInterstitial(shell()->tab()));
  }

  GURL ok_url() { return https_server_->GetURL("/simple_page.html"); }
  GURL bad_ssl_url() {
    return https_server_mismatched_->GetURL("/simple_page.html");
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_mismatched_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SSLBrowserTest);
};

// Tests clicking "take me back" on the interstitial page.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, TakeMeBack) {
  NavigateToOkPage();
  NavigateToPageWithSslErrorExpectBlocked();

  // Click "Take me back".
  SendInterstitialNavigationCommandAndWait(false /*proceed*/);

  // Check that it's possible to navigate to a new page.
  NavigateToOtherOkPage();

  // Navigate to the bad SSL page again, an interstitial shows again (in
  // contrast to what would happen had the user chosen to proceed).
  NavigateToPageWithSslErrorExpectBlocked();
}

// Tests clicking "take me back" on the interstitial page when there's no
// navigation history. The user should be taken to a safe page (about:blank).
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, TakeMeBackEmptyNavigationHistory) {
  NavigateToPageWithSslErrorExpectBlocked();

  // Click "Take me back".
  SendInterstitialNavigationCommandAndWait(false /*proceed*/,
                                           GURL("about:blank"));
}

IN_PROC_BROWSER_TEST_F(SSLBrowserTest, Reload) {
  NavigateToOkPage();
  NavigateToPageWithSslErrorExpectBlocked();

  SendInterstitialReloadCommandAndWait();

  // TODO(blundell): Ideally we would fix the SSL error, reload, and verify
  // that the SSL interstitial isn't showing. However, currently this doesn't
  // work: Calling ResetSSLConfig() on |http_server_mismatched_| passing
  // CERT_OK does not cause future reloads or navigations to bad_ssl_url() to
  // succeed; they still fail and pop an interstitial. I verified that the
  // LoadCompletionObserver is in fact waiting for a new load, i.e., there is
  // actually a *new* SSL interstitial popped up. From looking at the
  // ResetSSLConfig() impl there shouldn't be any waiting or anything needed
  // within the client.
}

// Tests clicking proceed link on the interstitial page. This is a PRE_ test
// because it also acts as setup for the test below which verifies the behavior
// across restarts.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, PRE_Proceed) {
  NavigateToOkPage();
  NavigateToPageWithSslErrorExpectBlocked();
  SendInterstitialNavigationCommandAndWait(true /*proceed*/);

  // Go back to an OK page, then try to navigate again. The "Proceed" decision
  // should be saved, so no interstitial is shown this time.
  NavigateToOkPage();
  NavigateToPageWithSslErrorExpectNotBlocked();
}

// The proceed decision is not perpetuated across WebLayer sessions, i.e.
// WebLayer will block again when navigating to the same bad page that was
// previously proceeded through.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, Proceed) {
  NavigateToPageWithSslErrorExpectBlocked();
}

// Tests navigating away from the interstitial page.
IN_PROC_BROWSER_TEST_F(SSLBrowserTest, NavigateAway) {
  NavigateToOkPage();
  NavigateToPageWithSslErrorExpectBlocked();
  NavigateToOtherOkPage();
}

}  // namespace weblayer
