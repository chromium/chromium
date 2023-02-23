// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/command_line.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

// The purpose of this test is to verify Variations code is correctly wired up
// for WebLayer. It's not intended to replicate VariationsHttpHeadersBrowserTest
class WebLayerVariationsHttpBrowserTest : public WebLayerBrowserTest {
 public:
  WebLayerVariationsHttpBrowserTest()
      : https_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {}

  ~WebLayerVariationsHttpBrowserTest() override = default;

  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from "www.google.com" without an interstitial.
    command_line->AppendSwitch("ignore-certificate-errors");

    WebLayerBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    auto* variations_provider =
        variations::VariationsIdsProvider::GetInstance();
    variations_provider->ForceVariationIds({"12", "456", "t789"}, "");

    // The test makes requests to google.com which we want to redirect to the
    // test server.
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.RegisterRequestHandler(
        base::BindRepeating(&WebLayerVariationsHttpBrowserTest::RequestHandler,
                            base::Unretained(this)));

    ASSERT_TRUE(https_server_.Start());
  }

  GURL GetGoogleUrlWithPath(const std::string& path) const {
    return https_server_.GetURL("www.google.com", path);
  }

  GURL GetGoogleRedirectUrl1() const {
    return GetGoogleUrlWithPath("/redirect");
  }

  GURL GetGoogleRedirectUrl2() const {
    return GetGoogleUrlWithPath("/redirect2");
  }

  GURL GetExampleUrlWithPath(const std::string& path) const {
    return https_server_.GetURL("www.example.com", path);
  }

  GURL GetExampleUrl() const { return GetExampleUrlWithPath("/landing.html"); }

  // Returns whether a given |header| has been received for a |url|. If
  // |url| has not been observed, fails an EXPECT and returns false.
  bool HasReceivedHeader(const GURL& url, const std::string& header) {
    base::AutoLock lock(received_headers_lock_);
    auto it = received_headers_.find(url);
    EXPECT_TRUE(it != received_headers_.end());
    if (it == received_headers_.end())
      return false;
    return it->second.find(header) != it->second.end();
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    // Retrieve the host name (without port) from the request headers.
    std::string host = "";
    if (request.headers.find("Host") != request.headers.end())
      host = request.headers.find("Host")->second;
    if (host.find(':') != std::string::npos)
      host = host.substr(0, host.find(':'));

    // Recover the original URL of the request by replacing the host name in
    // request.GetURL() (which is 127.0.0.1) with the host name from the request
    // headers.
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    GURL original_url = request.GetURL().ReplaceComponents(replacements);

    {
      base::AutoLock lock(received_headers_lock_);
      // Memorize the request headers for this URL for later verification.
      received_headers_[original_url] = request.headers;
    }

    // Set up a test server that redirects according to the
    // following redirect chain:
    // https://www.google.com:<port>/redirect
    // --> https://www.google.com:<port>/redirect2
    // --> https://www.example.com:<port>/
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    if (request.relative_url == GetGoogleRedirectUrl1().path()) {
      http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
      http_response->AddCustomHeader("Location",
                                     GetGoogleRedirectUrl2().spec());
    } else if (request.relative_url == GetGoogleRedirectUrl2().path()) {
      http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
      http_response->AddCustomHeader("Location", GetExampleUrl().spec());
    } else if (request.relative_url == GetExampleUrl().path()) {
      http_response->set_code(net::HTTP_OK);
      http_response->set_content("hello");
      http_response->set_content_type("text/plain");
    } else {
      return nullptr;
    }
    return http_response;
  }

 protected:
  net::EmbeddedTestServer https_server_;

  base::Lock received_headers_lock_;

  // Stores the observed HTTP Request headers.
  std::map<GURL, net::test_server::HttpRequest::HeaderMap> received_headers_
      GUARDED_BY(received_headers_lock_);
};

// Verify in an integration test that the variations header (X-Client-Data) is
// attached to network requests to Google but stripped on redirects.
IN_PROC_BROWSER_TEST_F(WebLayerVariationsHttpBrowserTest,
                       DISABLED_TestStrippingHeadersFromResourceRequest) {
  OneShotNavigationObserver observer(shell());
  shell()->tab()->GetNavigationController()->Navigate(GetGoogleRedirectUrl1());
  observer.WaitForNavigation();

  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl1(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetGoogleRedirectUrl2(), "X-Client-Data"));
  EXPECT_TRUE(HasReceivedHeader(GetExampleUrl(), "Host"));
  EXPECT_FALSE(HasReceivedHeader(GetExampleUrl(), "X-Client-Data"));
}

}  // namespace weblayer
