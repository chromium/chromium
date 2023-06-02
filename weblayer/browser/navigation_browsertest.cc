// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "weblayer/test/weblayer_browser_test.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/browser.h"
#include "weblayer/public/browser_observer.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/navigation_observer.h"
#include "weblayer/public/new_tab_delegate.h"
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
  using PageLanguageDeterminedCallback =
      base::RepeatingCallback<void(Page*, std::string)>;

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

  void SetOnPageLanguageDeterminedCallback(
      PageLanguageDeterminedCallback callback) {
    on_page_language_determined_callback_ = std::move(callback);
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
  void OnPageLanguageDetermined(Page* page,
                                const std::string& language) override {
    if (on_page_language_determined_callback_)
      on_page_language_determined_callback_.Run(page, language);
  }

 private:
  raw_ptr<NavigationController> controller_;
  Callback started_callback_;
  Callback redirected_callback_;
  Callback completed_callback_;
  Callback failed_callback_;
  PageLanguageDeterminedCallback on_page_language_determined_callback_;
};

}  // namespace

class NavigationBrowserTest : public WebLayerBrowserTest {
 public:
  NavigationController* GetNavigationController() {
    return shell()->tab()->GetNavigationController();
  }
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
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

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       IsPageInitiatedTrueForWindowHistoryBack) {
  EXPECT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<OneShotNavigationObserver> observer =
      std::make_unique<OneShotNavigationObserver>(shell());
  GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  observer->WaitForNavigation();
  ASSERT_TRUE(observer->completed());
  EXPECT_FALSE(observer->is_page_initiated());

  observer = std::make_unique<OneShotNavigationObserver>(shell());
  GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("b.com", "/simple_page.html"));
  observer->WaitForNavigation();
  ASSERT_TRUE(observer->completed());
  EXPECT_FALSE(observer->is_page_initiated());

  observer = std::make_unique<OneShotNavigationObserver>(shell());
  shell()->tab()->ExecuteScript(
      u"window.history.back();", false,
      base::BindLambdaForTesting(
          [&](base::Value value) { LOG(ERROR) << "executescript result"; }));
  observer->WaitForNavigation();
  ASSERT_TRUE(observer->completed());
  EXPECT_TRUE(observer->is_page_initiated());
}

// Http client error when the server returns a non-empty response.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, HttpClientError) {
  EXPECT_TRUE(embedded_test_server()->Start());

  OneShotNavigationObserver observer(shell());
  GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/non_empty404.html"));

  observer.WaitForNavigation();
  EXPECT_TRUE(observer.completed());
  EXPECT_FALSE(observer.is_error_page());
  EXPECT_EQ(observer.load_error(), Navigation::kHttpClientError);
  EXPECT_EQ(observer.http_status_code(), 404);
  EXPECT_EQ(observer.navigation_state(), NavigationState::kComplete);
}

// Http client error when the server returns an empty response.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, HttpClientErrorEmptyResponse) {
  EXPECT_TRUE(embedded_test_server()->Start());

  OneShotNavigationObserver observer(shell());
  GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/empty404.html"));

  observer.WaitForNavigation();
  EXPECT_FALSE(observer.completed());
  EXPECT_TRUE(observer.is_error_page());
  EXPECT_EQ(observer.load_error(), Navigation::kHttpClientError);
  EXPECT_EQ(observer.http_status_code(), 404);
  EXPECT_EQ(observer.navigation_state(), NavigationState::kFailed);
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// https://crbug.com/1296643
#define MAYBE_Download DISABLED_Download
#else
#define MAYBE_Download Download
#endif

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, MAYBE_Download) {
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
        // Destroying the tab posts a task to delete the WebContents, which must
        // be run before the test shuts down lest it access deleted state.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, run_loop.QuitClosure());
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

class BrowserObserverImpl : public BrowserObserver {
 public:
  explicit BrowserObserverImpl(Browser* browser) : browser_(browser) {
    browser->AddObserver(this);
  }

  ~BrowserObserverImpl() override { browser_->RemoveObserver(this); }

  void SetNewTabCallback(base::RepeatingCallback<void(Tab*)> callback) {
    new_tab_callback_ = callback;
  }

  // BrowserObserver:
  void OnTabAdded(Tab* tab) override { new_tab_callback_.Run(tab); }

 private:
  base::RepeatingCallback<void(Tab*)> new_tab_callback_;
  raw_ptr<Browser> browser_;
};

class NewTabDelegateImpl : public NewTabDelegate {
 public:
  // NewTabDelegate:
  void OnNewTab(Tab* new_tab, NewTabType type) override {}
};

// Ensures calling Navigate() from within NavigationStarted() for a popup does
// not crash.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, NavigateFromNewWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/simple_page2.html"), shell());
  NewTabDelegate* old_new_tab_delegate =
      static_cast<TabImpl*>(shell()->tab())->new_tab_delegate();
  NewTabDelegateImpl new_tab_delegate;
  shell()->tab()->SetNewTabDelegate(&new_tab_delegate);
  BrowserObserverImpl browser_observer(shell()->tab()->GetBrowser());
  std::unique_ptr<NavigationObserverImpl> popup_navigation_observer;
  base::RunLoop run_loop;
  Tab* popup_tab = nullptr;
  auto popup_started_navigation = [&](Navigation* navigation) {
    if (navigation->GetURL().path() == "/simple_page.html") {
      popup_tab->GetNavigationController()->Navigate(
          embedded_test_server()->GetURL("/simple_page3.html"));
    } else if (navigation->GetURL().path() == "/simple_page3.html") {
      run_loop.Quit();
    }
  };
  browser_observer.SetNewTabCallback(base::BindLambdaForTesting([&](Tab* tab) {
    popup_tab = tab;
    popup_navigation_observer = std::make_unique<NavigationObserverImpl>(
        tab->GetNavigationController());
    popup_navigation_observer->SetStartedCallback(
        base::BindLambdaForTesting(popup_started_navigation));
  }));
  // 'noopener' is key to triggering the problematic case.
  const std::string window_open = base::StringPrintf(
      "window.open('%s', '', 'noopener')",
      embedded_test_server()->GetURL("/simple_page.html").spec().c_str());
  ExecuteScriptWithUserGesture(shell()->tab(), window_open);
  run_loop.Run();

  // Restore the old delegate to make sure it is cleaned up on Android.
  shell()->tab()->SetNewTabDelegate(old_new_tab_delegate);
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

// Like above but checks that referer isn't sent when it's https and the target
// url is http.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       SetRequestHeaderWithRefererDowngrade) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string header_name = "Referer";
  const std::string header_value = "https://request.com";
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetRequestHeader(header_name, header_value);
      }));

  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  response.WaitForRequest();

  EXPECT_EQ(0u, response.http_request()->headers.count(header_name));
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

class NavigationBrowserTestUserAgentOverrideSubstring
    : public NavigationBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({blink::features::kUACHOverrideBlank},
                                          {});
    NavigationBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NavigationBrowserTestUserAgentOverrideSubstring,
                       PageSeesUserAgentString) {
  net::test_server::EmbeddedTestServer https_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));

  ASSERT_TRUE(https_server.Start());

  const std::string custom_ua = "custom";
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetUserAgentString(custom_ua);
      }));
  OneShotNavigationObserver navigation_observer(shell());
  shell()->LoadURL(https_server.GetURL("/simple_page.html"));
  navigation_observer.WaitForNavigation();

  base::RunLoop run_loop;
  shell()->tab()->ExecuteScript(
      u"navigator.userAgent;", false,
      base::BindLambdaForTesting([&](base::Value value) {
        ASSERT_TRUE(value.is_string());
        EXPECT_EQ(custom_ua, value.GetString());
        run_loop.Quit();
      }));
  run_loop.Run();

  // Ensure that userAgentData is blank when custom user agent is set.
  base::RunLoop run_loop2;
  shell()->tab()->ExecuteScript(
      u"navigator.userAgentData.platform;", false,
      base::BindLambdaForTesting([&](base::Value value) {
        ASSERT_TRUE(value.is_string());
        EXPECT_EQ("", value.GetString());
        run_loop2.Quit();
      }));
  run_loop2.Run();
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, Reload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  OneShotNavigationObserver observer(shell());
  GetNavigationController()->Navigate(
      embedded_test_server()->GetURL("/simple_page.html"));
  observer.WaitForNavigation();

  OneShotNavigationObserver observer2(shell());
  shell()->tab()->ExecuteScript(u"location.reload();", false,
                                base::DoNothing());
  observer2.WaitForNavigation();
  EXPECT_TRUE(observer2.completed());
  EXPECT_TRUE(observer2.is_reload());
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTestUserAgentOverrideSubstring,
                       SetUserAgentString) {
  std::unique_ptr<net::test_server::EmbeddedTestServer> https_server =
      std::make_unique<net::test_server::EmbeddedTestServer>(
          net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  https_server->AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));

  net::test_server::ControllableHttpResponse response_1(https_server.get(), "",
                                                        true);
  net::test_server::ControllableHttpResponse response_2(https_server.get(), "",
                                                        true);
  ASSERT_TRUE(https_server->Start());

  const std::string custom_ua = "CUSTOM";
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetUserAgentString(custom_ua);
      }));

  shell()->LoadURL(https_server->GetURL("/simple_page.html"));
  response_1.WaitForRequest();

  // |custom_ua| should be present in initial request.
  ASSERT_TRUE(base::Contains(response_1.http_request()->headers,
                             net::HttpRequestHeaders::kUserAgent));
  const std::string new_header = response_1.http_request()->headers.at(
      net::HttpRequestHeaders::kUserAgent);
  EXPECT_EQ(custom_ua, new_header);

  ASSERT_TRUE(base::Contains(response_1.http_request()->headers, "sec-ch-ua"));
  const std::string new_ch_header =
      response_1.http_request()->headers.at("Sec-CH-UA");
  EXPECT_EQ("", new_ch_header);
  content::FetchHistogramsFromChildProcesses();

  // Header should carry through to redirect.
  response_1.Send(
      "HTTP/1.1 302 Moved Temporarily\r\nLocation: /new_doc\r\n\r\n");
  response_1.Done();
  response_2.WaitForRequest();
  EXPECT_EQ(custom_ua, response_2.http_request()->headers.at(
                           net::HttpRequestHeaders::kUserAgent));
  EXPECT_EQ("", response_2.http_request()->headers.at("Sec-CH-UA"));
}

#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       SetUserAgentStringDoesntChangeViewportMetaTag) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigationObserverImpl observer(GetNavigationController());
  const std::string custom_ua = "custom-ua";
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetUserAgentString(custom_ua);
      }));

  OneShotNavigationObserver load_observer(shell());
  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  load_observer.WaitForNavigation();

  // Just because we set a custom user agent doesn't mean we should ignore
  // viewport meta tags.
  auto* tab = static_cast<TabImpl*>(shell()->tab());
  auto* web_contents = tab->web_contents();
  ASSERT_TRUE(web_contents->GetOrCreateWebPreferences().viewport_meta_enabled);
}

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       RequestDesktopSiteChangesViewportMetaTag) {
  ASSERT_TRUE(embedded_test_server()->Start());

  OneShotNavigationObserver load_observer(shell());
  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  load_observer.WaitForNavigation();

  auto* tab = static_cast<TabImpl*>(shell()->tab());

  OneShotNavigationObserver load_observer2(shell());
  tab->SetDesktopUserAgentEnabled(nullptr, true);
  load_observer2.WaitForNavigation();

  auto* web_contents = tab->web_contents();
  ASSERT_FALSE(web_contents->GetOrCreateWebPreferences().viewport_meta_enabled);
}

#endif

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

// https://crbug.com/1446050
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       DISABLED_SetUserAgentStringRendererInitiated) {
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
      u"location.href='" + base::ASCIIToUTF16(target_url.spec()) + u"';", false,
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
  // There's no notification to watch that would signal video wasn't autoplayed,
  // so instead check once through javascript.
  ASSERT_EQ(false, content::EvalJs(web_contents,
                                   "!document.getElementById('vid').paused"));
}

namespace {

class WaitForMediaPlaying : public content::WebContentsObserver {
 public:
  explicit WaitForMediaPlaying(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  WaitForMediaPlaying(const WaitForMediaPlaying&) = delete;
  WaitForMediaPlaying& operator=(const WaitForMediaPlaying&) = delete;

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
  auto main_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  https_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&, main_task_runner](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto iter = request.headers.find(variations::kClientDataHeader);
        if (iter != request.headers.end()) {
          main_task_runner->PostTask(
              FROM_HERE, base::BindOnce(base::BindLambdaForTesting(
                                            [&](const std::string& value) {
                                              last_header_value = value;
                                              run_loop->Quit();
                                            }),
                                        iter->second));
        }
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
  auto main_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  https_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&, main_task_runner](const net::test_server::HttpRequest& request)
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
          main_task_runner->PostTask(
              FROM_HERE, base::BindOnce(base::BindLambdaForTesting(
                                            [&](const std::string& value) {
                                              last_header_value = value;
                                              run_loop->Quit();
                                            }),
                                        iter->second));
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
  auto main_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  https_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&, main_task_runner](const net::test_server::HttpRequest& request)
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
          main_task_runner->PostTask(
              FROM_HERE, base::BindOnce(base::BindLambdaForTesting(
                                            [&](const std::string& value) {
                                              last_header_value = value;
                                              run_loop->Quit();
                                            }),
                                        iter->second));
        }
        return response;
      }));
  ASSERT_TRUE(https_server()->Start());

  const std::string header_value = "value";
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

#if BUILDFLAG(IS_ANDROID)
// Verifies setting the 'referer' to an android-app url works.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest, AndroidAppReferer) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  const std::string header_name = "Referer";
  const std::string header_value = "android-app://google.com/";
  NavigationObserverImpl observer(GetNavigationController());
  observer.SetStartedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        navigation->SetRequestHeader(header_name, header_value);
      }));

  shell()->LoadURL(embedded_test_server()->GetURL("/simple_page.html"));
  response.WaitForRequest();

  // Verify 'referer' matches expected value.
  EXPECT_EQ(header_value, response.http_request()->headers.at(header_name));
}
#endif

IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       OnPageLanguageDeterminedCallback) {
  ASSERT_TRUE(embedded_test_server()->Start());

  NavigationController* controller = shell()->tab()->GetNavigationController();
  NavigationObserverImpl observer(controller);

  Page* committed_page = nullptr;
  Page* page_with_language_determined = nullptr;
  std::string determined_language = "";

  base::RunLoop navigation_run_loop1;
  base::RunLoop page_language_determination_run_loop1;

  base::RunLoop* navigation_run_loop = &navigation_run_loop1;
  base::RunLoop* page_language_determination_run_loop =
      &page_language_determination_run_loop1;

  observer.SetCompletedClosure(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        committed_page = navigation->GetPage();
        navigation_run_loop->Quit();
      }));
  observer.SetOnPageLanguageDeterminedCallback(
      base::BindLambdaForTesting([&](Page* page, std::string language) {
        page_with_language_determined = page;
        determined_language = language;
        page_language_determination_run_loop->Quit();
      }));

  // Navigate to a page in English.
  controller->Navigate(embedded_test_server()->GetURL("/english_page.html"));
  navigation_run_loop1.Run();
  EXPECT_TRUE(committed_page);

  // Verify that the language determined event fires as expected.
  page_language_determination_run_loop1.Run();
  EXPECT_EQ(committed_page, page_with_language_determined);
  EXPECT_EQ("en", determined_language);

  // Now navigate to a page in French.
  committed_page = nullptr;
  page_with_language_determined = nullptr;
  base::RunLoop navigation_run_loop2;
  base::RunLoop page_language_determination_run_loop2;

  navigation_run_loop = &navigation_run_loop2;
  page_language_determination_run_loop = &page_language_determination_run_loop2;

  controller->Navigate(embedded_test_server()->GetURL("/french_page.html"));
  navigation_run_loop2.Run();
  EXPECT_TRUE(committed_page);

  // Verify that the language determined event fires as expected.
  page_language_determination_run_loop2.Run();
  EXPECT_EQ(committed_page, page_with_language_determined);
  EXPECT_EQ("fr", determined_language);
}

// Verifies that closing a tab when a navigation is waiting for a response
// causes the navigation to be marked as failed to the embedder.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       CloseTabWithNavigationWaitingForResponse) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/initial_url.html");
  base::RunLoop run_loop;
  Navigation* ongoing_navigation = nullptr;

  auto observer =
      std::make_unique<NavigationObserverImpl>(GetNavigationController());
  observer->SetStartedCallback(base::BindLambdaForTesting(
      [&](Navigation* navigation) { ongoing_navigation = navigation; }));
  observer->SetFailedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        EXPECT_EQ(url, navigation->GetURL());
        EXPECT_EQ(NavigationState::kFailed, navigation->GetState());

        run_loop.Quit();

        // The NavigationControllerImpl that |observer| is observing will
        // be destroyed before control returns to the test, so destroy
        // |observer| now to avoid UaF.
        observer.reset();
      }));

  shell()->LoadURL(url);
  response.WaitForRequest();

  EXPECT_EQ(NavigationState::kWaitingResponse, ongoing_navigation->GetState());
  shell()->browser()->DestroyTab(shell()->tab());

  run_loop.Run();
}

// Verifies that closing a tab when a navigation is in the middle of receiving a
// response causes the navigation to be marked as failed to the embedder.
IN_PROC_BROWSER_TEST_F(NavigationBrowserTest,
                       CloseTabWithNavigationReceivingBytes) {
  net::test_server::ControllableHttpResponse response(embedded_test_server(),
                                                      "", true);
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/initial_url.html");
  base::RunLoop run_loop;
  Navigation* ongoing_navigation = nullptr;

  auto observer =
      std::make_unique<NavigationObserverImpl>(GetNavigationController());
  observer->SetStartedCallback(base::BindLambdaForTesting(
      [&](Navigation* navigation) { ongoing_navigation = navigation; }));
  observer->SetFailedCallback(
      base::BindLambdaForTesting([&](Navigation* navigation) {
        EXPECT_EQ(url, navigation->GetURL());
        EXPECT_EQ(NavigationState::kFailed, navigation->GetState());

        run_loop.Quit();

        // The NavigationControllerImpl that |observer| is observing will
        // be destroyed before control returns to the test, so destroy
        // |observer| now to avoid UaF.
        observer.reset();
      }));

  auto* tab = static_cast<TabImpl*>(shell()->tab());
  auto wait_for_response_start =
      std::make_unique<content::TestNavigationManager>(tab->web_contents(),
                                                       url);
  shell()->LoadURL(url);
  // Wait until request is ready to start.
  EXPECT_TRUE(wait_for_response_start->WaitForRequestStart());
  // Start the request.
  wait_for_response_start->ResumeNavigation();
  // Wait for the request to arrive to ControllableHttpResponse.
  response.WaitForRequest();

  response.Send(net::HTTP_OK, "text/html", "<html>");

  ASSERT_TRUE(wait_for_response_start->WaitForResponse());

  EXPECT_EQ(NavigationState::kReceivingBytes, ongoing_navigation->GetState());

  // Destroy |wait_for_response_start| before we indirectly destroy the
  // WebContents it's observing.
  wait_for_response_start.reset();

  shell()->browser()->DestroyTab(tab);

  run_loop.Run();
}

}  // namespace weblayer
