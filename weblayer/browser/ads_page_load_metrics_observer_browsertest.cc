// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/page_load_metrics/browser/ads_page_load_metrics_test_waiter.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/frame_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/common/content_switches.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/subresource_filter_browser_test_harness.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

const char kCrossOriginHistogramId[] =
    "PageLoad.Clients.Ads.FrameCounts.AdFrames.PerFrame."
    "OriginStatus";

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
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricEmbedded) {
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
  histogram_tester.ExpectUniqueSample(kCrossOriginHistogramId,
                                      ad_metrics::OriginStatus::kSame, 1);
}

// Test that an empty embedded ad isn't reported at all.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricEmbeddedEmpty) {
  base::HistogramTester histogram_tester;
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL(
          "/ads_observer/srcdoc_embedded_ad_empty.html"),
      shell());
  NavigateAndWaitForCompletion(GURL(url::kAboutBlankURL), shell());
  histogram_tester.ExpectTotalCount(kCrossOriginHistogramId, 0);
}

// Test that an ad with the same origin as the main page is same origin.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricSame) {
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
  histogram_tester.ExpectUniqueSample(kCrossOriginHistogramId,
                                      ad_metrics::OriginStatus::kSame, 1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
      static_cast<int>(ad_metrics::OriginStatus::kSame));
}

// Test that an ad with a different origin as the main page is cross origin.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       OriginStatusMetricCross) {
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

  // Wait until all resource data updates are sent.
  waiter->AddPageExpectation(
      page_load_metrics::PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  waiter->Wait();
  NavigateAndWaitForCompletion(GURL(url::kAboutBlankURL), shell());
  histogram_tester.ExpectUniqueSample(kCrossOriginHistogramId,
                                      ad_metrics::OriginStatus::kCross, 1);
  auto entries =
      ukm_recorder.GetEntriesByName(ukm::builders::AdFrameLoad::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(), ukm::builders::AdFrameLoad::kStatus_CrossOriginName,
      static_cast<int>(ad_metrics::OriginStatus::kCross));
}

// This test harness does not start the test server and allows
// ControllableHttpResponses to be declared.
class AdsPageLoadMetricsObserverResourceBrowserTest
    : public SubresourceFilterBrowserTest {
 public:
  AdsPageLoadMetricsObserverResourceBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{subresource_filter::kAdTagging, {}}}, {});
  }

  ~AdsPageLoadMetricsObserverResourceBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();

    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_script.js"),
         subresource_filter::testing::CreateSuffixRule("ad_script_2.js")});
  }

 protected:
  std::unique_ptr<page_load_metrics::AdsPageLoadMetricsTestWaiter>
  CreateAdsPageLoadMetricsTestWaiter() {
    return std::make_unique<page_load_metrics::AdsPageLoadMetricsTestWaiter>(
        web_contents());
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

}  // namespace weblayer
