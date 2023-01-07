// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/heavy_ad_intervention/heavy_ad_features.h"
#include "components/heavy_ad_intervention/heavy_ad_service.h"
#include "components/page_load_metrics/browser/ads_page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/ad_intervention_browser_test_utils.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/frame_tree_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "weblayer/browser/heavy_ad_service_factory.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/subresource_filter_browser_test_harness.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

const char kAdsInterventionRecordedHistogram[] =
    "SubresourceFilter.PageLoad.AdsInterventionTriggered";

const char kCrossOriginHistogramId[] =
    "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
    "OriginStatus";

const char kHeavyAdInterventionTypeHistogramId[] =
    "PageLoad.Clients.Ads.HeavyAds.InterventionType2";

}  // namespace

class AdsPageLoadMetricsObserverBrowserTest
    : public SubresourceFilterBrowserTest {
 public:
  AdsPageLoadMetricsObserverBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{subresource_filter::kAdTagging, {}}}, {});
  }

  ~AdsPageLoadMetricsObserverBrowserTest() override = default;

  std::unique_ptr<page_load_metrics::PageLoadMetricsTestWaiter>
  CreatePageLoadMetricsTestWaiter() {
    return std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
        web_contents());
  }

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js")});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that an embedded ad is same origin.
// TODO(crbug.com/1210190): This test is flaky.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_OriginStatusMetricEmbedded) {
  base::HistogramTester histogram_tester;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/ads_observer/srcdoc_embedded_ad.html"),
      shell());
  // NOTE: The corresponding test in //chrome waits for 4 resources; the fourth
  // resource waited for is a favicon fetch that doesn't happen in WebLayer.
  waiter->AddMinimumCompleteResourcesExpectation(3);
  waiter->Wait();
  NavigateAndWaitForCompletion(GURL(url::kAboutBlankURL), shell());
  histogram_tester.ExpectUniqueSample(
      kCrossOriginHistogramId, page_load_metrics::OriginStatus::kSame, 1);
}

// Test that an empty embedded ad isn't reported at all.
// TODO(crbug.com/1226500): This test is flaky.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_OriginStatusMetricEmbeddedEmpty) {
  base::HistogramTester histogram_tester;
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL(
          "/ads_observer/srcdoc_embedded_ad_empty.html"),
      shell());
  NavigateAndWaitForCompletion(GURL(url::kAboutBlankURL), shell());
  histogram_tester.ExpectTotalCount(kCrossOriginHistogramId, 0);
}

// Test that an ad with the same origin as the main page is same origin.
// TODO(crbug.com/1210190): This test is flaky.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_OriginStatusMetricSame) {
  // Set the frame's resource as a rule.
  SetRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule("pixel.png")});
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/ads_observer/same_origin_ad.html"),
      shell());
  // NOTE: The corresponding test in //chrome waits for 4 resources; the fourth
  // resource waited for is a favicon fetch that doesn't // happen in WebLayer.
  waiter->AddMinimumCompleteResourcesExpectation(3);
  waiter->Wait();

  NavigateAndWaitForCompletion(GURL(url::kAboutBlankURL), shell());
  histogram_tester.ExpectUniqueSample(
      kCrossOriginHistogramId, page_load_metrics::OriginStatus::kSame, 1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
      static_cast<int>(page_load_metrics::OriginStatus::kSame));
}

// Test that an ad with a different origin as the main page is cross origin.
// TODO(crbug.com/1210190): This test is flaky.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_OriginStatusMetricCross) {
  // Note: Cannot navigate cross-origin without dynamically generating the URL.
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto waiter = CreatePageLoadMetricsTestWaiter();

  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL("/iframe_blank.html"), shell());
  // Note that the initial iframe is not an ad, so the metric doesn't observe
  // it initially as same origin.  However, on re-navigating to a cross
  // origin site that has an ad at its origin, the ad on that page is cross
  // origin from the original page.
  NavigateIframeToURL(web_contents(), "test",
                      embedded_test_server()->GetURL(
                          "a.com", "/ads_observer/same_origin_ad.html"));

  // Wait until all resource data updates are sent. Note that there is one more
  // than in the tests above due to the navigation to same_origin_ad.html being
  // itself made in an iframe.
  waiter->AddMinimumCompleteResourcesExpectation(4);
  waiter->Wait();
  NavigateAndWaitForCompletion(GURL(url::kAboutBlankURL), shell());
  histogram_tester.ExpectUniqueSample(
      kCrossOriginHistogramId, page_load_metrics::OriginStatus::kCross, 1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
      static_cast<int>(page_load_metrics::OriginStatus::kCross));
}

// This test harness does not start the test server and allows
// ControllableHttpResponses to be declared.
class AdsPageLoadMetricsObserverResourceBrowserTest
    : public SubresourceFilterBrowserTest {
 public:
  AdsPageLoadMetricsObserverResourceBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{subresource_filter::kAdsInterventionsEnforced, {}},
         {subresource_filter::kAdTagging, {}},
         {heavy_ad_intervention::features::kHeavyAdIntervention, {}},
         {heavy_ad_intervention::features::kHeavyAdPrivacyMitigations,
          {{"host-threshold", "3"}}}},
        {});
  }

  ~AdsPageLoadMetricsObserverResourceBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();

    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_script.js"),
         subresource_filter::testing::CreateSuffixRule("ad_script_2.js"),
         subresource_filter::testing::CreateSuffixRule("ad_iframe_writer.js")});
  }

 protected:
  std::unique_ptr<page_load_metrics::AdsPageLoadMetricsTestWaiter>
  CreateAdsPageLoadMetricsTestWaiter() {
    return std::make_unique<page_load_metrics::AdsPageLoadMetricsTestWaiter>(
        web_contents());
  }

  // This function loads a |large_resource| and if |will_block| is set, then
  // checks to see the resource is blocked, otherwise, it uses the |waiter| to
  // wait until the resource is loaded.
  void LoadHeavyAdResourceAndWaitOrError(
      net::test_server::ControllableHttpResponse* large_resource,
      page_load_metrics::PageLoadMetricsTestWaiter* waiter,
      bool will_block) {
    // Create a frame for the large resource.
    EXPECT_TRUE(ExecJs(web_contents(),
                       "createAdFrame('/ads_observer/"
                       "ad_with_incomplete_resource.html', '');"));

    if (will_block) {
      // If we expect the resource to be blocked, load a resource large enough
      // to trigger the intervention and ensure that the navigation failed.
      content::TestNavigationObserver error_observer(
          web_contents(), net::ERR_BLOCKED_BY_CLIENT);
      page_load_metrics::LoadLargeResource(
          large_resource, page_load_metrics::kMaxHeavyAdNetworkSize);
      error_observer.WaitForNavigationFinished();
      EXPECT_FALSE(error_observer.last_navigation_succeeded());
    } else {
      // Otherwise load the resource, ensuring enough bytes were loaded.
      int64_t current_network_bytes = waiter->current_network_bytes();
      page_load_metrics::LoadLargeResource(
          large_resource, page_load_metrics::kMaxHeavyAdNetworkSize);
      waiter->AddMinimumNetworkBytesExpectation(
          current_network_bytes + page_load_metrics::kMaxHeavyAdNetworkSize);
      waiter->Wait();
    }
  }

 private:
  // SubresourceFilterBrowserTest:
  bool StartEmbeddedTestServerAutomatically() override { return false; }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       ReceivedAdResources) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();

  NavigateAndWaitForCompletion(embedded_test_server()->GetURL(
                                   "foo.com", "/ad_tagging/frame_factory.html"),
                               shell());

  // Two subresources should have been reported as ads.
  waiter->AddMinimumAdResourceExpectation(2);
  waiter->Wait();
}

// Verifies that the frame is navigated to the intervention page when a
// heavy ad intervention triggers.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       HeavyAdInterventionEnabled_ErrorPageLoaded) {
  base::HistogramTester histogram_tester;
  auto incomplete_resource_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a navigation observer that will watch for the intervention to
  // navigate the frame.
  content::TestNavigationObserver error_observer(web_contents(),
                                                 net::ERR_BLOCKED_BY_CLIENT);

  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  GURL url = embedded_test_server()->GetURL(
      "/ads_observer/ad_with_incomplete_resource.html");
  NavigateAndWaitForCompletion(url, shell());

  // Load a resource large enough to trigger the intervention.
  page_load_metrics::LoadLargeResource(
      incomplete_resource_response.get(),
      page_load_metrics::kMaxHeavyAdNetworkSize);

  // Wait for the intervention page navigation to finish on the frame.
  error_observer.WaitForNavigationFinished();

  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 1);

  // Check that the ad frame was navigated to the intervention page.
  EXPECT_FALSE(error_observer.last_navigation_succeeded());

  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 1);
  histogram_tester.ExpectBucketCount(
      "Blink.UseCounter.Features",
      blink::mojom::WebFeature::kHeavyAdIntervention, 1);
}

// Check that the Heavy Ad Intervention fires the correct number of times to
// protect privacy, and that after that limit is hit, the Ads Intervention
// Framework takes over for future navigations.
// TODO(crbug.com/1210190): This test is flaky.
IN_PROC_BROWSER_TEST_F(
    AdsPageLoadMetricsObserverResourceBrowserTest,
    DISABLED_HeavyAdInterventionBlocklistFull_InterventionBlocked) {
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      http_responses(4);
  for (auto& http_response : http_responses) {
    http_response =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/ads_observer/incomplete_resource.js",
            false /*relative_url_is_prefix*/);
  }
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a waiter for the navigation and navigate.
  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  NavigateAndWaitForCompletion(embedded_test_server()->GetURL(
                                   "foo.com", "/ad_tagging/frame_factory.html"),
                               shell());

  // Load and block the resource. The ads intervention framework should not
  // be triggered at this point.
  LoadHeavyAdResourceAndWaitOrError(http_responses[0].get(), waiter.get(),
                                    /*will_block=*/true);
  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 1);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);

  // Block a second resource on the page. The ads intervention framework should
  // not be triggered at this point.
  LoadHeavyAdResourceAndWaitOrError(http_responses[1].get(), waiter.get(),
                                    /*will_block=*/true);
  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 2);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);

  // Create a new waiter for the next navigation and navigate.
  waiter = CreateAdsPageLoadMetricsTestWaiter();
  // Set query to ensure that it's not treated as a reload as preview metrics
  // are not recorded for reloads.
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL(
          "foo.com", "/ad_tagging/frame_factory.html?avoid_reload"),
      shell());

  // Load and block the resource. The ads intervention framework should
  // be triggered at this point.
  LoadHeavyAdResourceAndWaitOrError(http_responses[2].get(), waiter.get(),
                                    /*will_block=*/true);
  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 3);
  histogram_tester.ExpectUniqueSample(
      kAdsInterventionRecordedHistogram,
      subresource_filter::mojom::AdsViolation::kHeavyAdsInterventionAtHostLimit,
      1);
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);

  // Reset the waiter and navigate again. Check we show the Ads Intervention UI.
  waiter.reset();
  NavigateAndWaitForCompletion(embedded_test_server()->GetURL(
                                   "foo.com", "/ad_tagging/frame_factory.html"),
                               shell());
  histogram_tester.ExpectUniqueSample(
      kAdsInterventionRecordedHistogram,
      subresource_filter::mojom::AdsViolation::kHeavyAdsInterventionAtHostLimit,
      1);
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 1);
}

// Check that clearing browsing data resets the number of times that the Heavy
// Ad Intervention has been triggered.
// TODO(crbug.com/1210190): This test is flaky.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceBrowserTest,
                       DISABLED_ClearBrowsingDataClearsHeavyAdBlocklist) {
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      http_responses(4);
  for (auto& http_response : http_responses) {
    http_response =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/ads_observer/incomplete_resource.js",
            false /*relative_url_is_prefix*/);
  }
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a waiter for the navigation and navigate.
  auto waiter = CreateAdsPageLoadMetricsTestWaiter();
  NavigateAndWaitForCompletion(embedded_test_server()->GetURL(
                                   "foo.com", "/ad_tagging/frame_factory.html"),
                               shell());

  // Load and block the resource. The ads intervention framework should not
  // be triggered at this point.
  LoadHeavyAdResourceAndWaitOrError(http_responses[0].get(), waiter.get(),
                                    /*will_block=*/true);
  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 1);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);

  // Block a second resource on the page. The ads intervention framework should
  // not be triggered at this point.
  LoadHeavyAdResourceAndWaitOrError(http_responses[1].get(), waiter.get(),
                                    /*will_block=*/true);
  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 2);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);

  // Clear browsing data and wait for the heavy ad blocklist to be cleared and
  // reloaded (note that waiting for the latter event is necessary as until the
  // blocklist is loaded all hosts are treated as blocklisted, which
  // interferes with the flow below).
  auto* heavy_ad_service = HeavyAdServiceFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext());

  base::RunLoop on_browsing_data_cleared_run_loop;
  base::RunLoop on_blocklist_cleared_run_loop;
  base::RunLoop on_blocklist_reloaded_run_loop;

  // First clear browsing data and wait for the blocklist to be cleared.
  heavy_ad_service->NotifyOnBlocklistCleared(
      on_blocklist_cleared_run_loop.QuitClosure());
  base::Time now = base::Time::Now();
  GetProfile()->ClearBrowsingData(
      {BrowsingDataType::COOKIES_AND_SITE_DATA}, now - base::Days(1), now,
      on_browsing_data_cleared_run_loop.QuitClosure());
  on_blocklist_cleared_run_loop.Run();

  // Then wait for the blocklist to be reloaded before proceeding.
  heavy_ad_service->NotifyOnBlocklistLoaded(
      on_blocklist_reloaded_run_loop.QuitClosure());
  on_blocklist_reloaded_run_loop.Run();

  // Create a new waiter for the next navigation and navigate.
  waiter = CreateAdsPageLoadMetricsTestWaiter();
  // Set query to ensure that it's not treated as a reload as preview metrics
  // are not recorded for reloads.
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL(
          "foo.com", "/ad_tagging/frame_factory.html?avoid_reload"),
      shell());

  // Load and block the resource. The ads intervention framework should not be
  // triggered at this point as the heavy ad blocklist was cleared as part of
  // clearing browsing data.
  LoadHeavyAdResourceAndWaitOrError(http_responses[2].get(), waiter.get(),
                                    /*will_block=*/true);
  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 3);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);

  // Reset the waiter and navigate again. Check we don't show the Ads
  // Intervention UI.
  waiter.reset();
  NavigateAndWaitForCompletion(embedded_test_server()->GetURL(
                                   "foo.com", "/ad_tagging/frame_factory.html"),
                               shell());
  // Note that the below metric will not have been updated due to this
  // navigation being trated as a reload.
  histogram_tester.ExpectUniqueSample(
      kHeavyAdInterventionTypeHistogramId,
      page_load_metrics::HeavyAdStatus::kNetwork, 3);
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
  histogram_tester.ExpectBucketCount(
      "SubresourceFilter.Actions2",
      subresource_filter::SubresourceFilterAction::kUIShown, 0);
}

class AdsPageLoadMetricsObserverResourceIncognitoBrowserTest
    : public AdsPageLoadMetricsObserverResourceBrowserTest {
 public:
  AdsPageLoadMetricsObserverResourceIncognitoBrowserTest() {
    SetShellStartsInIncognitoMode();
  }
};

// Verifies that the blocklist is setup correctly and the intervention triggers
// in incognito mode.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverResourceIncognitoBrowserTest,
                       HeavyAdInterventionIncognitoMode_InterventionFired) {
  base::HistogramTester histogram_tester;
  auto incomplete_resource_response =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), "/ads_observer/incomplete_resource.js",
          true /*relative_url_is_prefix*/);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a navigation observer that will watch for the intervention to
  // navigate the frame.
  content::TestNavigationObserver error_observer(web_contents(),
                                                 net::ERR_BLOCKED_BY_CLIENT);

  // Create a waiter for the incognito contents.
  auto waiter = std::make_unique<page_load_metrics::PageLoadMetricsTestWaiter>(
      web_contents());
  GURL url = embedded_test_server()->GetURL(
      "/ads_observer/ad_with_incomplete_resource.html");
  NavigateAndWaitForCompletion(url, shell());

  // Load a resource large enough to trigger the intervention.
  page_load_metrics::LoadLargeResource(
      incomplete_resource_response.get(),
      page_load_metrics::kMaxHeavyAdNetworkSize);

  // Wait for the intervention page navigation to finish on the frame.
  error_observer.WaitForNavigationFinished();

  // Check that the ad frame was navigated to the intervention page.
  EXPECT_FALSE(error_observer.last_navigation_succeeded());
}

}  // namespace weblayer
