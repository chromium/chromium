// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/subresource_filter/content/browser/ad_tagging_browser_test_utils.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/test/subresource_filter_browser_test_harness.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

// Tests of ad tagging integration in WebLayer. A minimal port of //chrome's
// ad_tagging_browsertest.cc.
class AdTaggingBrowserTest : public SubresourceFilterBrowserTest {
 public:
  AdTaggingBrowserTest() = default;
  ~AdTaggingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    SubresourceFilterBrowserTest::SetUpOnMainThread();

    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("ad_script.js"),
         subresource_filter::testing::CreateSuffixRule("ad=true")});
  }

  GURL GetURL(const std::string& page) {
    return embedded_test_server()->GetURL("/ad_tagging/" + page);
  }
};

IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       DISABLED_AdContentSettingAllowed_AdTaggingDisabled) {
  HostContentSettingsMapFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext())
      ->SetDefaultContentSetting(ContentSettingsType::ADS,
                                 CONTENT_SETTING_ALLOW);

  subresource_filter::TestSubresourceFilterObserver observer(web_contents());
  NavigateAndWaitForCompletion(GetURL("frame_factory.html"), shell());

  // Create an ad frame.
  GURL ad_url = GetURL("frame_factory.html?2&ad=true");
  content::RenderFrameHost* ad_frame =
      subresource_filter::CreateSrcFrame(web_contents(), ad_url);

  // Verify that we are not evaluating subframe loads.
  EXPECT_FALSE(observer.GetChildFrameLoadPolicy(ad_url).has_value());
  EXPECT_FALSE(observer.GetIsAdFrame(ad_frame->GetFrameTreeNodeId()));

  // Child frame created by ad script.
  content::RenderFrameHost* ad_frame_tagged_by_script =
      subresource_filter::CreateSrcFrameFromAdScript(
          web_contents(), GetURL("frame_factory.html?1"));

  // No frames should be detected by script heuristics.
  EXPECT_FALSE(
      observer.GetIsAdFrame(ad_frame_tagged_by_script->GetFrameTreeNodeId()));
}

// TODO(crbug.com/1210190): This test is flaky.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest,
                       DISABLED_AdContentSettingBlocked_AdTaggingEnabled) {
  HostContentSettingsMapFactory::GetForBrowserContext(
      web_contents()->GetBrowserContext())
      ->SetDefaultContentSetting(ContentSettingsType::ADS,
                                 CONTENT_SETTING_BLOCK);

  subresource_filter::TestSubresourceFilterObserver observer(web_contents());
  NavigateAndWaitForCompletion(GetURL("frame_factory.html"), shell());

  // Create an ad frame.
  GURL ad_url = GetURL("frame_factory.html?2&ad=true");
  content::RenderFrameHost* ad_frame =
      subresource_filter::CreateSrcFrame(web_contents(), ad_url);

  // Verify that we are evaluating subframe loads.
  EXPECT_TRUE(observer.GetChildFrameLoadPolicy(ad_url).has_value());
  EXPECT_TRUE(observer.GetIsAdFrame(ad_frame->GetFrameTreeNodeId()));

  // Child frame created by ad script.
  content::RenderFrameHost* ad_frame_tagged_by_script =
      subresource_filter::CreateSrcFrameFromAdScript(
          web_contents(), GetURL("frame_factory.html?1"));

  // Frames should be detected by script heuristics.
  EXPECT_TRUE(
      observer.GetIsAdFrame(ad_frame_tagged_by_script->GetFrameTreeNodeId()));
}

// TODO(crbug.com/1210190): This test is flaky.
IN_PROC_BROWSER_TEST_F(AdTaggingBrowserTest, DISABLED_FramesByURL) {
  subresource_filter::TestSubresourceFilterObserver observer(web_contents());

  // Main frame.
  NavigateAndWaitForCompletion(GetURL("frame_factory.html"), shell());
  EXPECT_FALSE(observer.GetIsAdFrame(
      web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId()));

  // (1) Vanilla child.
  content::RenderFrameHost* vanilla_child = subresource_filter::CreateSrcFrame(
      web_contents(), GetURL("frame_factory.html?1"));
  EXPECT_FALSE(observer.GetIsAdFrame(vanilla_child->GetFrameTreeNodeId()));

  // (2) Ad child.
  content::RenderFrameHost* ad_child = subresource_filter::CreateSrcFrame(
      web_contents(), GetURL("frame_factory.html?2&ad=true"));
  EXPECT_TRUE(observer.GetIsAdFrame(ad_child->GetFrameTreeNodeId()));
  EXPECT_TRUE(subresource_filter::EvidenceForFrameComprises(
      ad_child, /*parent_is_ad=*/false,
      blink::mojom::FilterListResult::kMatchedBlockingRule,
      blink::mojom::FrameCreationStackEvidence::kNotCreatedByAdScript));

  // (3) Ad child of 2.
  content::RenderFrameHost* ad_child_2 = subresource_filter::CreateSrcFrame(
      ad_child, GetURL("frame_factory.html?sub=1&3&ad=true"));
  EXPECT_TRUE(observer.GetIsAdFrame(ad_child_2->GetFrameTreeNodeId()));
  EXPECT_TRUE(subresource_filter::EvidenceForFrameComprises(
      ad_child_2, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedBlockingRule,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  // (4) Vanilla child of 2.
  content::RenderFrameHost* vanilla_child_2 =
      subresource_filter::CreateSrcFrame(ad_child,
                                         GetURL("frame_factory.html?4"));
  EXPECT_TRUE(observer.GetIsAdFrame(vanilla_child_2->GetFrameTreeNodeId()));
  EXPECT_TRUE(subresource_filter::EvidenceForFrameComprises(
      vanilla_child_2, /*parent_is_ad=*/true,
      blink::mojom::FilterListResult::kMatchedNoRules,
      blink::mojom::FrameCreationStackEvidence::kCreatedByAdScript));

  // (5) Vanilla child of 1. This tests something subtle.
  // frame_factory.html?ad=true loads the same script that frame_factory.html
  // uses to load frames. This tests that even though the script is tagged as an
  // ad in the ad iframe, it's not considered an ad in the main frame, hence
  // it's able to create an iframe that's not labeled as an ad.
  content::RenderFrameHost* vanilla_child_3 =
      subresource_filter::CreateSrcFrame(vanilla_child,
                                         GetURL("frame_factory.html?5"));
  EXPECT_FALSE(observer.GetIsAdFrame(vanilla_child_3->GetFrameTreeNodeId()));
}

}  // namespace weblayer
