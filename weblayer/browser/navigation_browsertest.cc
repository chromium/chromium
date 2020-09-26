// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/test/bind_test_util.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/browser.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/navigation_observer.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/interstitial_utils.h"
#include "weblayer/test/test_navigation_observer.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

// NavigationObserver that allows registering a callback for various
// NavigationObserver functions.
class NavigationObserverImpl : public NavigationObserver {
 public:
  explicit NavigationObserverImpl(NavigationController* controller)
      : controller_(controller) {
    controller_->AddObserver(this);
  }
  ~NavigationObserverImpl() override { controller_->RemoveObserver(this); }

  using Callback = base::RepeatingCallback<void(Navigation*)>;

  void SetStartedCallback(Callback callback) {
    started_callback_ = std::move(callback);
  }

  void SetRedirectedCallback(Callback callback) {
    redirected_callback_ = std::move(callback);
  }

  void SetFailedCallback(Callback callback) {
    failed_callback_ = std::move(callback);
  }

  void SetCompletedClosure(Callback callback) {
    completed_callback_ = std::move(callback);
  }

  // NavigationObserver:
  void NavigationStarted(Navigation* navigation) override {
    if (started_callback_)
      started_callback_.Run(navigation);
  }
  void NavigationRedirected(Navigation* navigation) override {
    if (redirected_callback_)
      redirected_callback_.Run(navigation);
  }
  void NavigationCompleted(Navigation* navigation) override {
    if (completed_callback_)
      completed_callback_.Run(navigation);
  }
  void NavigationFailed(Navigation* navigation) override {
    // As |this| may be deleted when running the callback, the callback must be
    // copied before running. To do otherwise results in use-after-free.
    auto callback = failed_callback_;
    if (callback)
      callback.Run(navigation);
  }

 private:
  NavigationController* controller_;
  Callback started_callback_;
  Callback redirected_callback_;
  Callback completed_callback_;
  Callback failed_callback_;
};

}  // namespace

class NavigationBrowserTest : public WebLayerBrowserTest {
 public:
  NavigationController* GetNavigationController() {
    return shell()->tab()->GetNavigationController();
  }
};

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, NoError) {
  EXPECT_TRUE(embedded_test_server()->Start());

  OneShotNavigationObserver observer(shell());
  GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/simple_page.html"));

  observer.WaitForNavigation();
  EXPECT_TRUE(observer.completed());
  EXPECT_FALSE(observer.is_error_page());
  EXPECT_FALSE(observer.is_download());
  EXPECT_FALSE(observer.is_reload());
  EXPECT_FALSE(observer.was_stop_called());
  EXPECT_EQ(observer.load_error(), Navigation::kNoError);
  EXPECT_EQ(observer.http_status_code(), 200);
  EXPECT_EQ(observer.navigation_state(), NavigationState::kComplete);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, HttpClientError) {
  EXPECT_TRUE(embedded_test_server()->Start());

  OneShotNavigationObserver observer(shell());
  GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/non_existent.html"));

  observer.WaitForNavigation();
  EXPECT_TRUE(observer.completed());
  EXPECT_FALSE(observer.is_error_page());
  EXPECT_EQ(observer.load_error(), Navigation::kHttpClientError);
  EXPECT_EQ(observer.http_status_code(), 404);
  EXPECT_EQ(observer.navigation_state(), NavigationState::kComplete);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, HttpServerError) {
  EXPECT_TRUE(embedded_test_server()->Start());

  OneShotNavigationObserver observer(shell());
  GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/echo?status=500"));

  observer.WaitForNavigation();
  EXPECT_TRUE(observer.completed());
  EXPECT_FALSE(observer.is_error_page());
  EXPECT_EQ(observer.load_error(), Navigation::kHttpServerError);
  EXPECT_EQ(observer.http_status_code(), 500);
  EXPECT_EQ(observer.navigation_state(), NavigationState::kComplete);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, SSLError) {
  net::EmbeddedTestServer https_server_mismatched(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_mismatched.SetSSLConfig(
      net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server_mismatched.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));

  ASSERT_TRUE(https_server_mismatched.Start());

  OneShotNavigationObserver observer(shell());
  GetNavigationController()->Navigate(
      https_server_mismatched.GetURL("/simple_page.html"));

  observer.WaitForNavigation();
  EXPECT_FALSE(observer.completed());
  EXPECT_TRUE(observer.is_error_page());
  EXPECT_EQ(observer.load_error(), Navigation::kSSLError);
  EXPECT_EQ(observer.navigation_state(), NavigationState::kFailed);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, HttpConnectivityError) {
  GURL url("http://doesntexist.com/foo");
  auto interceptor = content::URLLoaderInterceptor::SetupRequestFailForURL(
      url, net::ERR_NAME_NOT_RESOLVED);

  OneShotNavigationObserver observer(shell());
  GetNavigationController()->Navigate(url);

  observer.WaitForNavigation();
  EXPECT_FALSE(observer.completed());
  EXPECT_TRUE(observer.is_error_page());
  EXPECT_EQ(observer.load_error(), Navigation::kConnectivityError);
  EXPECT_EQ(observer.navigation_state(), NavigationState::kFailed);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, Download) {
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/content-disposition.html"));

  OneShotNavigationObserver observer(shell());
  GetNavigationController()->Navigate(url);

  observer.WaitForNavigation();
  EXPECT_FALSE(observer.completed());
  EXPECT_FALSE(observer.is_error_page());
  EXPECT_TRUE(observer.is_download());
  EXPECT_FALSE(observer.was_stop_called());
  EXPECT_EQ(observer.load_error(), Navigation::kOtherError);
  EXPECT_EQ(observer.navigation_state(), NavigationState::kFailed);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, StopInOnStart) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::RunLoop run_loop;
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(base::BindLambdaForTesting(
      [&](Navigation*) { GetNavigationController()->Stop(); }));
  observer.SetFailedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        ASSERT_TRUE(navigation->WasStopCalled());
        run_loop.Quit();
      }));
  GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/simple_page.html"));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, DestroyTabInNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  Tab* new_tab = shell()->browser()->CreateTab();
  base::RunLoop run_loop;
  std::unique_ptr<NavigationObserverImpl> observer =
      std::make_unique<NavigationObserverImpl>(
          new_tab->GetNavigationController());
  observer->SetFailedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        observer.reset();
        shell()->browser()->DestroyTab(new_tab);
        run_loop.Quit();
      }));
  new_tab->GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/simple_pageX.html"));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, StopInOnRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::RunLoop run_loop;
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetRedirectedCallback(base::BindLambdaForTesting(
      [&](Navigation*) { GetNavigationController()->Stop(); }));
  observer.SetFailedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        ASSERT_TRUE(navigation->WasStopCalled());
        run_loop.Quit();
      }));
  const GURL original_url = embedded_test_server()->GetURL("/simple_page.html");
  GetNavigationController()->Navigate(embedded_test_server()->GetURL(
      "/server-redirect?" + original_url.spec()));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       NavigateFromRendererInitiatedNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigationController* controller = shell()->tab()->GetNavigationController();
  const GURL final_url = embedded_test_server()->GetURL("/simple_page2.html");
  int failed_count = 0;
  int completed_count = 0;
  NavigationObserverImpl observer(controller);
  base::RunLoop run_loop;
  observer.SetFailedCallback(
      base::BindLambdaForTesting([&](Navigation*) { failed_count++; }));
  observer.SetCompletedClosure(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        completed_count++;
        if (navigation->GetURL().path() == "/simple_page2.html")
          run_loop.Quit();
      }));
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        if (navigation->GetURL().path() == "/simple_page.html")
          controller->Navigate(final_url);
      }));
  controller->Navigate(embedded_test_server()->GetURL("/simple_page4.html"));
  run_loop.Run();
  EXPECT_EQ(1, failed_count);
  EXPECT_EQ(2, completed_count);
  ASSERT_EQ(2, controller->GetNavigationListSize());
  EXPECT_EQ(final_url, controller->GetNavigationEntryDisplayURL(1));
}

// Verifies calling Navigate() from NavigationRedirected() works.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, NavigateFromRedirect) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigationObserverImpl observer(GetNavigationController());
  bool got_redirect = false;
  const GURL url_to_load_on_redirect =
      embedded_test_server()->GetURL("/url_to_load_on_redirect.html");
  observer.SetRedirectedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        shell()->LoadURL(url_to_load_on_redirect);
        got_redirect = true;
      }));

  shell()->LoadURL(embedded_test_server()->GetURL("/initial_url.html"));
  response_1.WaitForRequest();
  response_1.Send(
      "HTTP/1.1 302 Moved Temporarily\r\nLocation: /redirect_dest_url\r\n\r\n");
  response_1.Done();
  response_2.WaitForRequest();
  response_2.Done();
  EXPECT_EQ(url_to_load_on_redirect, response_2.http_request()->GetURL());
  EXPECT_TRUE(got_redirect);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, SetRequestHeader) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string header_name = "header";
  const std::string header_value = "value";
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetRequestHeader(header_name, header_value);
      }));

  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  response_1.WaitForRequest();

  // Header should be present in initial request.
  EXPECT_EQ(header_value, response_1.http_request()->headers.at(header_name));
  response_1.Send(
      "HTTP/1.1 302 Moved Temporarily\r\nLocation: /new_doc\r\n\r\n");
  response_1.Done();

  // Header should carry through to redirect.
  response_2.WaitForRequest();
  EXPECT_EQ(header_value, response_2.http_request()->headers.at(header_name));
}

// Verifies setting the 'referer' via SetRequestHeader() works as expected.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, SetRequestHeaderWithReferer) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string header_name = "Referer";
  const std::string header_value = "http://request.com";
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetRequestHeader(header_name, header_value);
      }));

  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  response.WaitForRequest();

  // Verify 'referer' matches expected value.
  EXPECT_EQ(GURL(header_value),
            GURL(response.http_request()->headers.at(header_name)));
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, SetRequestHeaderInRedirect) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string header_name = "header";
  const std::string header_value = "value";
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetRedirectedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetRequestHeader(header_name, header_value);
      }));
  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  response_1.WaitForRequest();

  // Header should not be present in initial request.
  EXPECT_FALSE(base::Contains(response_1.http_request()->headers, header_name));

  response_1.Send(
      "HTTP/1.1 302 Moved Temporarily\r\nLocation: /new_doc\r\n\r\n");
  response_1.Done();

  response_2.WaitForRequest();

  // Header should be in redirect.
  ASSERT_TRUE(base::Contains(response_2.http_request()->headers, header_name));
  EXPECT_EQ(header_value, response_2.http_request()->headers.at(header_name));
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, PageSeesUserAgentString) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string custom_ua = "custom";
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetUserAgentString(custom_ua);
      }));
  OneShotNavigationObserver navigation_observer(shell());
  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  navigation_observer.WaitForNavigation();

  base::RunLoop run_loop;
  shell()->tab()->ExecuteScript(
      base::ASCIIToUTF16("navigator.userAgent;"), false,
      base::BindLambdaForTesting([&](base::Value value) {
        ASSERT_TRUE(value.is_string());
        EXPECT_EQ(custom_ua, value.GetString());
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, Reload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  OneShotNavigationObserver observer(shell());
  GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/simple_page.html"));
  observer.WaitForNavigation();

  OneShotNavigationObserver observer2(shell());
  shell()->tab()->ExecuteScript(base::ASCIIToUTF16("location.reload();"), false,
                                base::DoNothing());
  observer2.WaitForNavigation();
  EXPECT_TRUE(observer2.completed());
  EXPECT_TRUE(observer2.is_reload());
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, SetUserAgentString) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string custom_ua = "CUSTOM";
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetUserAgentString(custom_ua);
      }));

  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  response_1.WaitForRequest();

  // |custom_ua| should be present in initial request.
  ASSERT_TRUE(base::Contains(response_1.http_request()->headers,
                             net::HttpRequestHeaders::kUserAgent));
  const std::string new_header = response_1.http_request()->headers.at(
      net::HttpRequestHeaders::kUserAgent);
  EXPECT_EQ(custom_ua, new_header);

  // Header should carry through to redirect.
  response_1.Send(
      "HTTP/1.1 302 Moved Temporarily\r\nLocation: /new_doc\r\n\r\n");
  response_1.Done();
  response_2.WaitForRequest();
  EXPECT_EQ(custom_ua, response_2.http_request()->headers.at(
                           net::HttpRequestHeaders::kUserAgent));
}

// Verifies changing the user agent twice in a row works.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, UserAgentDoesntCarryThrough1) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string custom_ua1 = "my ua1";
  const std::string custom_ua2 = "my ua2";
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetUserAgentString(custom_ua1);
      }));

  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  response_1.WaitForRequest();
  EXPECT_EQ(custom_ua1, response_1.http_request()->headers.at(
                            net::HttpRequestHeaders::kUserAgent));

  // Before the request is done, start another navigation.
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetUserAgentString(custom_ua2);
      }));
  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page2.html"));
  response_2.WaitForRequest();
  EXPECT_EQ(custom_ua2, response_2.http_request()->headers.at(
                            net::HttpRequestHeaders::kUserAgent));
}

// Verifies changing the user agent doesn't bleed through to next navigation.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, UserAgentDoesntCarryThrough2) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string custom_ua = "my ua1";
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetUserAgentString(custom_ua);
      }));

  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  response_1.WaitForRequest();
  EXPECT_EQ(custom_ua, response_1.http_request()->headers.at(
                           net::HttpRequestHeaders::kUserAgent));

  // Before the request is done, start another navigation.
  observer.SetStartedCallback(base::DoNothing());
  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page2.html"));
  response_2.WaitForRequest();
  EXPECT_NE(custom_ua, response_2.http_request()->headers.at(
                           net::HttpRequestHeaders::kUserAgent));
  EXPECT_FALSE(response_2.http_request()
                   ->headers.at(net::HttpRequestHeaders::kUserAgent)
                   .empty());
}

// Verifies changing the user-agent applies to child resources, such as an
// <img>.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       UserAgentAppliesToChildResources) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string custom_ua = "custom-ua";
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetUserAgentString(custom_ua);
      }));

  shell()->LoadURL(embedded_test_server()->GetURL("/foo.html"));
  response_1.WaitForRequest();
  response_1.Send(net::HTTP_OK, "text/html", "<img src=\"image.png\">");
  response_1.Done();
  EXPECT_EQ(custom_ua, response_1.http_request()->headers.at(
                           net::HttpRequestHeaders::kUserAgent));
  observer.SetStartedCallback(base::DoNothing());

  response_2.WaitForRequest();
  EXPECT_EQ(custom_ua, response_2.http_request()->headers.at(
                           net::HttpRequestHeaders::kUserAgent));
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       SetUserAgentStringRendererInitiated) {
  net::test_server::ControllableHttpResponse response_1(embedded_test_server(),
                                                        "", true);
  net::test_server::ControllableHttpResponse response_2(embedded_test_server(),
                                                        "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  OneShotNavigationObserver load_observer(shell());
  NavigationObserverImpl observer(GetNavigationController());
  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  response_1.WaitForRequest();
  response_1.Send(net::HTTP_OK, "text/html", "<html>");
  response_1.Done();
  load_observer.WaitForNavigation();

  const std::string custom_ua = "custom-ua";
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetUserAgentString(custom_ua);
      }));
  const GURL target_url = embedded_test_server()->GetURL("/foo.html");
  shell()->tab()->ExecuteScript(
      base::ASCIIToUTF16("location.href='" + target_url.spec() + "';"), false,
      base::DoNothing());
  response_2.WaitForRequest();
  // |custom_ua| should be present in the renderer initiated navigation.
  ASSERT_TRUE(base::Contains(response_2.http_request()->headers,
                             net::HttpRequestHeaders::kUserAgent));
  const std::string new_ua = response_2.http_request()->headers.at(
      net::HttpRequestHeaders::kUserAgent);
  EXPECT_EQ(custom_ua, new_ua);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, AutoPlayDefault) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/autoplay.html"));
  auto* tab = static_cast<TabImpl*>(shell()->tab());
  NavigateAndWaitForCompletion(url, tab);

  auto* web_contents = tab->web_contents();
  bool playing = false;
  // There's no notification to watch that would signal video wasn't autoplayed,
  // so instead check once through javascript.
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send(!document.getElementById('vid')."
      "paused)",
      &playing));
  ASSERT_FALSE(playing);
}

namespace {

class WaitForMediaPlaying : public content::WebContentsObserver {
 public:
  explicit WaitForMediaPlaying(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  // WebContentsObserver override.
  void MediaStartedPlaying(const MediaPlayerInfo& info,
                           const content::MediaPlayerId&) final {
    run_loop_.Quit();
    CHECK(info.has_audio);
    CHECK(info.has_video);
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WaitForMediaPlaying);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, AutoPlayEnabled) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/autoplay.html"));
  NavigationController::NavigateParams params;
  params.enable_auto_play = true;
  GetNavigationController()->Navigate(url, params);

  auto* tab = static_cast<TabImpl*>(shell()->tab());
  WaitForMediaPlaying wait_for_media(tab->web_contents());
  wait_for_media.Wait();
}

class NavigationBrowserTest2 : public NavigationBrowserTest {
 public:
  void SetUp() override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from "www.google.com" without an interstitial.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        "ignore-certificate-errors");

    NavigationBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    NavigationBrowserTest::SetUpOnMainThread();
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    // The test makes requests to google.com which we want to redirect to the
    // test server.
    host_resolver()->AddRule("*", "127.0.0.1");

    // Forces variations code to set the header.
    auto* variations_provider =
        variations::VariationsIdsProvider::GetInstance();
    variations_provider->ForceVariationIds({"12", "456", "t789"}, "");
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// This test verifies the embedder can replace the X-Client-Data header that
// is also set by //components/variations.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest2, ReplaceXClientDataHeader) {
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  std::string last_header_value;
  https_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto iter = request.headers.find(variations::kClientDataHeader);
        if (iter != request.headers.end())
          last_header_value = iter->second;
        run_loop->Quit();
        return std::make_unique<net::test_server::BasicHttpResponse>();
      }));
  ASSERT_TRUE(https_server()->Start());

  // Verify the header is set by default.
  const GURL url = https_server()->GetURL("www.google.com", "/");
  shell()->LoadURL(url);
  run_loop->Run();
  EXPECT_FALSE(last_header_value.empty());

  // Repeat, but clobber the header when navigating.
  const std::string header_value = "value";
  EXPECT_NE(last_header_value, header_value);
  last_header_value.clear();
  run_loop = std::make_unique<base::RunLoop>();
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetRequestHeader(variations::kClientDataHeader,
                                     header_value);
      }));

  shell()->LoadURL(https_server()->GetURL("www.google.com", "/foo"));
  run_loop->Run();
  EXPECT_EQ(header_value, last_header_value);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest2,
                       SetXClientDataHeaderCarriesThroughToRedirect) {
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  std::string last_header_value;
  bool should_redirect = true;
  https_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        if (should_redirect) {
          should_redirect = false;
          response->set_code(net::HTTP_MOVED_PERMANENTLY);
          response->AddCustomHeader(
              "Location",
              https_server()->GetURL("www.google.com", "/redirect").spec());
        } else {
          auto iter = request.headers.find(variations::kClientDataHeader);
          last_header_value = iter->second;
          run_loop->Quit();
        }
        return response;
      }));
  ASSERT_TRUE(https_server()->Start());

  const std::string header_value = "value";
  run_loop = std::make_unique<base::RunLoop>();
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetRequestHeader(variations::kClientDataHeader,
                                     header_value);
      }));

  shell()->LoadURL(https_server()->GetURL("www.google.com", "/foo"));
  run_loop->Run();
  EXPECT_EQ(header_value, last_header_value);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest2, SetXClientDataHeaderInRedirect) {
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  std::string last_header_value;
  bool should_redirect = true;
  https_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        if (should_redirect) {
          should_redirect = false;
          response->set_code(net::HTTP_MOVED_PERMANENTLY);
          response->AddCustomHeader(
              "Location",
              https_server()->GetURL("www.google.com", "/redirect").spec());
        } else {
          auto iter = request.headers.find(variations::kClientDataHeader);
          last_header_value = iter->second;
          run_loop->Quit();
        }
        return response;
      }));
  ASSERT_TRUE(https_server()->Start());

  const std::string header_value = "value";
  run_loop = std::make_unique<base::RunLoop>();
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetRedirectedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetRequestHeader(variations::kClientDataHeader,
                                     header_value);
      }));

  shell()->LoadURL(https_server()->GetURL("www.google.com", "/foo"));
  run_loop->Run();
  EXPECT_EQ(header_value, last_header_value);
}

}  // namespace weblayer
