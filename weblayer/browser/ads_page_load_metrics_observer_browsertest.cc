// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "components/page_load_metrics/browser/ads_page_load_metrics_test_waiter.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "net/dns/mock_host_resolver.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class AdsPageLoadMetricsObserverBrowserTest : public WebLayerBrowserTest {
 public:
  AdsPageLoadMetricsObserverBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{subresource_filter::kAdTagging, {}}}, {});
  }

  ~AdsPageLoadMetricsObserverBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_script.js"),
         subresource_filter::testing::CreateSuffixRule("ad_script_2.js")});
  }

 protected:
  std::unique_ptr<page_load_metrics::AdsPageLoadMetricsTestWaiter>
  CreateAdsPageLoadMetricsTestWaiter() {
    auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();
    return std::make_unique<page_load_metrics::AdsPageLoadMetricsTestWaiter>(
        web_contents);
  }

 private:
  void SetRulesetWithRules(
      const std::vector<url_pattern_index::proto::UrlRule>& rules) {
    subresource_filter::testing::TestRulesetPair test_ruleset_pair;
    test_ruleset_creator_.CreateRulesetWithRules(rules, &test_ruleset_pair);

    subresource_filter::testing::TestRulesetPublisher test_ruleset_publisher(
        BrowserProcess::GetInstance()->subresource_filter_ruleset_service());
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));
  }

  subresource_filter::testing::TestRulesetCreator test_ruleset_creator_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/1183549): flaky.
IN_PROC_BROWSER_TEST_F(AdsPageLoadMetricsObserverBrowserTest,
                       DISABLED_ReceivedAdResources) {
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
