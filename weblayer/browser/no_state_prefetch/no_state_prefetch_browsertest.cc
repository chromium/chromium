// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/browser/prerender_histograms.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_monitor.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/resource_request.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_link_manager_factory.h"
#include "weblayer/browser/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/prerender_controller.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "weblayer/browser/android/metrics/metrics_test_helper.h"
#endif

namespace weblayer {

class NoStatePrefetchBrowserTest : public WebLayerBrowserTest {
 public:
#if BUILDFLAG(IS_ANDROID)
  void SetUp() override {
    InstallTestGmsBridge(ConsentType::kConsent);

    WebLayerBrowserTest::SetUp();
  }

  void TearDown() override {
    RemoveTestGmsBridge();
    WebLayerBrowserTest::TearDown();
  }
#endif

  void SetUpOnMainThread() override {
    prerendered_page_fetched_ = std::make_unique<base::RunLoop>();
    script_resource_fetched_ = std::make_unique<base::RunLoop>();

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &NoStatePrefetchBrowserTest::HandleRequest, base::Unretained(this)));
    https_server_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));
    ASSERT_TRUE(https_server_->Start());

#if BUILDFLAG(IS_ANDROID)
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
#endif
  }

  // Helper methods.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path().find("prerendered_page") != std::string::npos) {
      prerendered_page_fetched_->Quit();
      prerendered_page_was_fetched_ = true;
    }
    if (request.GetURL().path().find("prefetch.js") != std::string::npos) {
      script_fetched_ = true;
      auto iter = request.headers.find("Purpose");
      purpose_header_value_ = iter->second;
      script_resource_fetched_->Quit();
    }
    if (request.GetURL().path().find("prefetch_meta.js") != std::string::npos) {
      script_executed_ = true;
    }

    // The default handlers will take care of this request.
    return nullptr;
  }

  void NavigateToPageAndWaitForTitleChange(const GURL& navigate_to,
                                           std::u16string expected_title) {
    content::TitleWatcher title_watcher(
        static_cast<TabImpl*>(shell()->tab())->web_contents(), expected_title);
    NavigateAndWaitForCompletion(navigate_to, shell());
    ASSERT_TRUE(expected_title == title_watcher.WaitAndGetTitle());
  }

 protected:
  content::BrowserContext* GetBrowserContext() {
    Tab* tab = shell()->tab();
    TabImpl* tab_impl = static_cast<TabImpl*>(tab);
    return tab_impl->web_contents()->GetBrowserContext();
  }

  std::unique_ptr<base::RunLoop> prerendered_page_fetched_;
  std::unique_ptr<base::RunLoop> script_resource_fetched_;
  bool prerendered_page_was_fetched_ = false;
  bool script_fetched_ = false;
  bool script_executed_ = false;
  std::string purpose_header_value_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
#endif
};

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       CreateNoStatePrefetchManager) {
  auto* no_state_prefetch_manager =
      NoStatePrefetchManagerFactory::GetForBrowserContext(GetBrowserContext());
  EXPECT_TRUE(no_state_prefetch_manager);
}

IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       CreateNoStatePrefetchLinkManager) {
  auto* no_state_prefetch_link_manager =
      NoStatePrefetchLinkManagerFactory::GetForBrowserContext(
          GetBrowserContext());
  EXPECT_TRUE(no_state_prefetch_link_manager);
}

// Test that adding a link-rel prerender tag causes a fetch.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       LinkRelPrerenderPageFetched) {
  NavigateAndWaitForCompletion(GURL(https_server_->GetURL("/parent_page.html")),
                               shell());
  prerendered_page_fetched_->Run();
}

// Test that only render blocking resources are loaded during NoStatePrefetch.
// TODO(https://crbug.com/1144282): Fix failures on Asan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_NSPLoadsRenderBlockingResource \
  DISABLED_NSPLoadsRenderBlockingResource
#else
#define MAYBE_NSPLoadsRenderBlockingResource NSPLoadsRenderBlockingResource
#endif
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       MAYBE_NSPLoadsRenderBlockingResource) {
  NavigateAndWaitForCompletion(GURL(https_server_->GetURL("/parent_page.html")),
                               shell());
  script_resource_fetched_->Run();
  EXPECT_EQ("prefetch", purpose_header_value_);
  EXPECT_FALSE(script_executed_);
}

// Test that navigating to a no-state-prefetched page executes JS and reuses
// prerendered resources.
// TODO(https://crbug.com/1144282): Fix failures on Asan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_NavigateToPrerenderedPage DISABLED_NavigateToPrerenderedPage
#else
#define MAYBE_NavigateToPrerenderedPage NavigateToPrerenderedPage
#endif
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       MAYBE_NavigateToPrerenderedPage) {
  NavigateAndWaitForCompletion(GURL(https_server_->GetURL("/parent_page.html")),
                               shell());
  script_resource_fetched_->Run();

  // Navigate to the prerendered page and wait for its title to change.
  script_fetched_ = false;
  NavigateToPageAndWaitForTitleChange(
      GURL(https_server_->GetURL("/prerendered_page.html")), u"Prefetch Page");

  EXPECT_FALSE(script_fetched_);
  EXPECT_TRUE(script_executed_);
}

#if BUILDFLAG(IS_ANDROID)
// Test that no-state-prefetch results in UKM getting recorded.
// TODO(https://crbug.com/1292252): Flaky failures.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, DISABLED_UKMRecorded) {
  GetProfile()->SetBooleanSetting(SettingType::UKM_ENABLED, true);
  NavigateAndWaitForCompletion(GURL(https_server_->GetURL("/parent_page.html")),
                               shell());
  script_resource_fetched_->Run();

  NavigateToPageAndWaitForTitleChange(
      GURL(https_server_->GetURL("/prerendered_page.html")), u"Prefetch Page");

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::NoStatePrefetch::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  const auto* entry = entries[0];
  // FinalStatus must be set to FINAL_STATUS_NOSTATE_PREFETCH_FINISHED.
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::NoStatePrefetch::kPrefetchedRecently_FinalStatusName, 56);
  // Origin must be set to ORIGIN_LINK_REL_PRERENDER_SAMEDOMAIN.
  ukm_recorder_->ExpectEntryMetric(
      entry, ukm::builders::NoStatePrefetch::kPrefetchedRecently_OriginName, 7);
}
#endif

// link-rel="prerender" happens even when NoStatePrefetch has been disabled.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       LinkRelPrerenderWithNSPDisabled) {
  GetProfile()->SetBooleanSetting(SettingType::NETWORK_PREDICTION_ENABLED,
                                  false);
  NavigateAndWaitForCompletion(GURL(https_server_->GetURL("/parent_page.html")),
                               shell());
  prerendered_page_fetched_->Run();
}

// link-rel="next" URLs should not be prefetched.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, LinkRelNextWithNSPDisabled) {
  NavigateAndWaitForCompletion(
      GURL(https_server_->GetURL("/link_rel_next_parent.html")), shell());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(prerendered_page_was_fetched_);
}

// Non-web initiated prerender succeeds and subsequent navigations reuse
// previously downloaded resources.
// TODO(https://crbug.com/1144282): Fix failures on Asan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ExternalPrerender DISABLED_ExternalPrerender
#else
#define MAYBE_ExternalPrerender ExternalPrerender
#endif
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest, MAYBE_ExternalPrerender) {
  GetProfile()->GetPrerenderController()->Prerender(
      GURL(https_server_->GetURL("/prerendered_page.html")));

  script_resource_fetched_->Run();

  // Navigate to the prerendered page and wait for its title to change.
  script_fetched_ = false;
  NavigateToPageAndWaitForTitleChange(
      GURL(https_server_->GetURL("/prerendered_page.html")), u"Prefetch Page");
  EXPECT_FALSE(script_fetched_);
}

// Non-web initiated prerender fails when the user has opted out.
IN_PROC_BROWSER_TEST_F(NoStatePrefetchBrowserTest,
                       ExternalPrerenderWhenOptedOut) {
  GetProfile()->SetBooleanSetting(SettingType::NETWORK_PREDICTION_ENABLED,
                                  false);
  GetProfile()->GetPrerenderController()->Prerender(
      GURL(https_server_->GetURL("/prerendered_page.html")));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(prerendered_page_was_fetched_);
}

}  // namespace weblayer
