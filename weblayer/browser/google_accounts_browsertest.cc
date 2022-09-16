// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "google_apis/gaia/gaia_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/signin_url_loader_throttle.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/google_accounts_delegate.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/test_navigation_observer.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {
namespace {
constexpr char kGaiaDomain[] = "fakegaia.com";
constexpr char kGoogleAccountsPath[] = "/manageaccounts";
constexpr char kGoogleAccountsRedirectPath[] = "/manageaccounts-redirect";
}  // namespace

class GoogleAccountsBrowserTest : public WebLayerBrowserTest,
                                  public GoogleAccountsDelegate {
 public:
  // WebLayerBrowserTest:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    shell()->tab()->SetGoogleAccountsDelegate(this);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &GoogleAccountsBrowserTest::HandleGoogleAccountsRequest,
        base::Unretained(this)));
    net::test_server::RegisterDefaultHandlers(&https_server_);
    ASSERT_TRUE(https_server_.Start());
    command_line->AppendSwitchASCII(switches::kGaiaUrl, GetGaiaURL("/").spec());
    command_line->AppendSwitch("ignore-certificate-errors");
  }

  // GoogleAccountsDelegate:
  void OnGoogleAccountsRequest(
      const signin::ManageAccountsParams& params) override {
    params_ = params;
    if (run_loop_)
      run_loop_->Quit();
  }

  std::string GetGaiaId() override { return ""; }

  const signin::ManageAccountsParams& WaitForGoogleAccounts() {
    if (!params_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
    EXPECT_TRUE(params_.has_value());
    return params_.value();
  }

  GURL GetGaiaURL(const std::string& path) {
    return https_server_.GetURL(kGaiaDomain, path);
  }

  GURL GetNonGaiaURL(const std::string& path) {
    return https_server_.GetURL(path);
  }

  bool HasReceivedGoogleAccountsResponse() { return params_.has_value(); }

  std::string GetBody() {
    return ExecuteScript(shell(), "document.body.innerText", true).GetString();
  }

 protected:
  std::string response_header_;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleGoogleAccountsRequest(
      const net::test_server::HttpRequest& request) {
    std::string path = request.GetURL().path();
    if (path != kSignOutPath && path != kGoogleAccountsPath &&
        path != kGoogleAccountsRedirectPath) {
      return nullptr;
    }

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    if (path == kGoogleAccountsRedirectPath) {
      http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
      http_response->AddCustomHeader(
          "Location",
          base::UnescapeBinaryURLComponent(request.GetURL().query_piece()));
    } else {
      http_response->set_code(net::HTTP_OK);
    }

    if (base::Contains(request.headers, signin::kChromeConnectedHeader)) {
      http_response->AddCustomHeader(signin::kChromeManageAccountsHeader,
                                     response_header_);
    }
    http_response->set_content("");
    return http_response;
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  absl::optional<signin::ManageAccountsParams> params_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

IN_PROC_BROWSER_TEST_F(GoogleAccountsBrowserTest, Basic) {
  response_header_ =
      "action=ADDSESSION,email=foo@bar.com,"
      "continue_url=https://blah.com,is_same_tab=true";
  NavigateAndWaitForCompletion(GetGaiaURL(kGoogleAccountsPath), shell());
  const signin::ManageAccountsParams& params = WaitForGoogleAccounts();
  EXPECT_EQ(params.service_type, signin::GAIA_SERVICE_TYPE_ADDSESSION);
  EXPECT_EQ(params.email, "foo@bar.com");
  EXPECT_EQ(params.continue_url, "https://blah.com");
  EXPECT_TRUE(params.is_same_tab);
}

IN_PROC_BROWSER_TEST_F(GoogleAccountsBrowserTest, NonGaiaUrl) {
  response_header_ = "action=ADDSESSION";
  NavigateAndWaitForCompletion(GetNonGaiaURL(kGoogleAccountsPath), shell());

  // Navigate again to make sure the manage accounts response would have been
  // received.
  NavigateAndWaitForCompletion(GetNonGaiaURL("/echo"), shell());
  EXPECT_FALSE(HasReceivedGoogleAccountsResponse());
}

IN_PROC_BROWSER_TEST_F(GoogleAccountsBrowserTest, RedirectFromGaiaURL) {
  response_header_ = "action=SIGNUP";

  GURL final_url = GetGaiaURL(kGoogleAccountsPath);
  TestNavigationObserver test_observer(
      final_url, TestNavigationObserver::NavigationEvent::kCompletion,
      shell()->tab());
  shell()->tab()->GetNavigationController()->Navigate(GetNonGaiaURL(
      base::StrCat({kGoogleAccountsRedirectPath, "?", final_url.spec()})));
  test_observer.Wait();

  const signin::ManageAccountsParams& params = WaitForGoogleAccounts();
  EXPECT_EQ(params.service_type, signin::GAIA_SERVICE_TYPE_SIGNUP);
}

IN_PROC_BROWSER_TEST_F(GoogleAccountsBrowserTest, RedirectToGaiaURL) {
  response_header_ = "action=SIGNUP";

  GURL final_url = GetNonGaiaURL(kGoogleAccountsPath);
  TestNavigationObserver test_observer(
      final_url, TestNavigationObserver::NavigationEvent::kCompletion,
      shell()->tab());
  shell()->tab()->GetNavigationController()->Navigate(GetGaiaURL(
      base::StrCat({kGoogleAccountsRedirectPath, "?", final_url.spec()})));
  test_observer.Wait();

  const signin::ManageAccountsParams& params = WaitForGoogleAccounts();
  EXPECT_EQ(params.service_type, signin::GAIA_SERVICE_TYPE_SIGNUP);
}

IN_PROC_BROWSER_TEST_F(GoogleAccountsBrowserTest, AddsQueryParamsToSignoutURL) {
  response_header_ = "action=SIGNUP";

  // Sign out URL on the GAIA domain will get a query param added to it.
  GURL sign_out_url = GetGaiaURL(kSignOutPath);
  GURL sign_out_url_with_params =
      net::AppendQueryParameter(sign_out_url, "manage", "true");
  {
    TestNavigationObserver test_observer(
        sign_out_url_with_params,
        TestNavigationObserver::NavigationEvent::kCompletion, shell()->tab());
    shell()->tab()->GetNavigationController()->Navigate(sign_out_url);
    test_observer.Wait();
  }

  // Other URLs will not have query param added.
  NavigateAndWaitForCompletion(GetGaiaURL(kGoogleAccountsPath), shell());
  NavigateAndWaitForCompletion(GetNonGaiaURL(kSignOutPath), shell());

  // Redirecting to sign out URL will also add params.
  {
    TestNavigationObserver test_observer(
        sign_out_url_with_params,
        TestNavigationObserver::NavigationEvent::kCompletion, shell()->tab());
    shell()->tab()->GetNavigationController()->Navigate(
        GetNonGaiaURL("/server-redirect?" + sign_out_url.spec()));
    test_observer.Wait();
  }
}

IN_PROC_BROWSER_TEST_F(GoogleAccountsBrowserTest, AddsRequestHeaderToGaiaURLs) {
  const std::string path =
      base::StrCat({"/echoheader?", signin::kChromeConnectedHeader});
  NavigateAndWaitForCompletion(GetGaiaURL(path), shell());
  EXPECT_EQ(GetBody(),
            "source=WebLayer,mode=3,enable_account_consistency=true,"
            "consistency_enabled_by_default=false");

  // Non GAIA URL should not get the header.
  NavigateAndWaitForCompletion(GetNonGaiaURL(path), shell());
  EXPECT_EQ(GetBody(), "None");
}

class IncognitoGoogleAccountsBrowserTest : public GoogleAccountsBrowserTest {
 public:
  IncognitoGoogleAccountsBrowserTest() { SetShellStartsInIncognitoMode(); }
};

IN_PROC_BROWSER_TEST_F(IncognitoGoogleAccountsBrowserTest,
                       HeaderAddedForIncognitoBrowser) {
  const std::string path =
      base::StrCat({"/echoheader?", signin::kChromeConnectedHeader});
  NavigateAndWaitForCompletion(GetGaiaURL(path), shell());
  EXPECT_EQ(GetBody(),
            "source=WebLayer,mode=3,enable_account_consistency=true,"
            "consistency_enabled_by_default=false");
}

}  // namespace weblayer
