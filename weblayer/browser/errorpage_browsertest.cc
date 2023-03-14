// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "weblayer/test/weblayer_browser_test.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/embedder_support/switches.h"
#include "components/error_page/content/browser/net_error_auto_reloader.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace weblayer {

using ErrorPageBrowserTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(ErrorPageBrowserTest, NameNotResolved) {
  GURL error_page_url =
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);

  NavigateAndWaitForFailure(error_page_url, shell());

  // Currently, interstitials for error pages are displayed only on Android.
#if BUILDFLAG(IS_ANDROID)
  std::u16string expected_title =
      l10n_util::GetStringUTF16(IDS_ANDROID_ERROR_PAGE_WEBPAGE_NOT_AVAILABLE);
  EXPECT_EQ(expected_title, GetTitle(shell()));
#endif
}

// Verifies that navigating to a URL that returns a 404 with an empty body
// results in the navigation failing.
IN_PROC_BROWSER_TEST_F(ErrorPageBrowserTest, 404WithEmptyBody) {
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL error_page_url = embedded_test_server()->GetURL("/empty404.html");

  NavigateAndWaitForFailure(error_page_url, shell());
}

class ErrorPageReloadBrowserTest : public ErrorPageBrowserTest {
 public:
  ErrorPageReloadBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(embedder_support::kEnableAutoReload);
    ErrorPageBrowserTest::SetUpCommandLine(command_line);
  }

  // Helper to perform navigations, whether successful or intercepted for
  // simulated failure. Note that this asynchronously initiates the navigation
  // and then waits only for the *navigation* to finish; this is in contrast to
  // common test utilities which wait for loading to finish. It matters because
  // most of NetErrorAutoReloader's interesting behavior is triggered at
  // navigation completion and tests may want to observe the immediate side
  // effects, such as the scheduling of an auto-reload timer.
  //
  // Return true if the navigation was successful, or false if it failed.
  [[nodiscard]] bool Navigate(const GURL& url,
                              bool disable_network_error_auto_reload = false) {
    content::TestNavigationManager navigation(web_contents(), url);
    NavigationController::NavigateParams params;
    auto* navigation_controller = shell()->tab()->GetNavigationController();
    std::unique_ptr<DisableAutoReload> disable_auto_reload;
    if (disable_network_error_auto_reload)
      disable_auto_reload =
          std::make_unique<DisableAutoReload>(navigation_controller);
    navigation_controller->Navigate(url, params);
    EXPECT_TRUE(navigation.WaitForNavigationFinished());
    return navigation.was_successful();
  }

  // Returns the time-delay of the currently scheduled auto-reload task, if one
  // is scheduled. If no auto-reload is scheduled, this returns null.
  absl::optional<base::TimeDelta> GetCurrentAutoReloadDelay() {
    auto* auto_reloader =
        error_page::NetErrorAutoReloader::FromWebContents(web_contents());
    if (!auto_reloader)
      return absl::nullopt;
    const absl::optional<base::OneShotTimer>& timer =
        auto_reloader->next_reload_timer_for_testing();
    if (!timer)
      return absl::nullopt;
    return timer->GetCurrentDelay();
  }

  content::WebContents* web_contents() {
    return static_cast<TabImpl*>(shell()->tab())->web_contents();
  }

 private:
  class DisableAutoReload : public NavigationObserver {
   public:
    explicit DisableAutoReload(NavigationController* controller)
        : controller_(controller) {
      controller_->AddObserver(this);
    }
    ~DisableAutoReload() override { controller_->RemoveObserver(this); }

    // NavigationObserver implementation:
    void NavigationStarted(Navigation* navigation) override {
      navigation->DisableNetworkErrorAutoReload();
    }

   private:
    raw_ptr<NavigationController> controller_;
  };
};

IN_PROC_BROWSER_TEST_F(ErrorPageReloadBrowserTest, ReloadOnNetworkChanged) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Ensure that the NetErrorAutoReloader believes it's online, otherwise it
  // does not attempt auto-reload on error pages.
  error_page::NetErrorAutoReloader::CreateForWebContents(web_contents());
  auto* reloader =
      error_page::NetErrorAutoReloader::FromWebContents(web_contents());
  reloader->DisableConnectionChangeObservationForTesting();
  reloader->OnConnectionChanged(network::mojom::ConnectionType::CONNECTION_4G);

  GURL url = embedded_test_server()->GetURL("/error_page");
  // We send net::ERR_NETWORK_CHANGED on the first load, and the reload should
  // get a net::OK response.
  bool first_try = true;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&url, &first_try](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == url) {
          if (first_try) {
            first_try = false;
            params->client->OnComplete(
                network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED));
          } else {
            content::URLLoaderInterceptor::WriteResponse(
                "weblayer/test/data/simple_page.html", params->client.get());
          }
          return true;
        }
        return false;
      }));

  NavigateAndWaitForCompletion(url, shell());
}

// By default auto reload is enabled.
IN_PROC_BROWSER_TEST_F(ErrorPageReloadBrowserTest, AutoReloadDefault) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Ensure that the NetErrorAutoReloader believes it's online, otherwise it
  // does not attempt auto-reload on error pages.
  error_page::NetErrorAutoReloader::CreateForWebContents(web_contents());
  auto* reloader =
      error_page::NetErrorAutoReloader::FromWebContents(web_contents());
  reloader->DisableConnectionChangeObservationForTesting();
  reloader->OnConnectionChanged(network::mojom::ConnectionType::CONNECTION_4G);

  GURL url = embedded_test_server()->GetURL("/error_page");
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&url](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == url) {
          params->client->OnComplete(
              network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED));
          return true;
        }
        return false;
      }));

  EXPECT_FALSE(Navigate(url));
  EXPECT_EQ(error_page::NetErrorAutoReloader::GetNextReloadDelayForTesting(0),
            GetCurrentAutoReloadDelay());
}

IN_PROC_BROWSER_TEST_F(ErrorPageReloadBrowserTest, AutoReloadDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/error_page");
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&url](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == url) {
          params->client->OnComplete(
              network::URLLoaderCompletionStatus(net::ERR_NETWORK_CHANGED));
          return true;
        }
        return false;
      }));

  EXPECT_FALSE(Navigate(url, true));
  EXPECT_EQ(absl::nullopt, GetCurrentAutoReloadDelay());
}

}  // namespace weblayer
