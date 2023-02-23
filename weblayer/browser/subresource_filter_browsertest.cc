// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/subresource_filter/content/browser/ads_intervention_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/subresource_filter_profile_context_factory.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/grit/weblayer_resources.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/subresource_filter_browser_test_harness.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/infobars/android/infobar_android.h"  // nogncheck
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar_manager.h"  // nogncheck
#endif

namespace weblayer {

namespace {

const char kAdsInterventionRecordedHistogram[] =
    "SubresourceFilter.PageLoad.AdsInterventionTriggered";
const char kTimeSinceAdsInterventionTriggeredHistogram[] =
    "SubresourceFilter.PageLoad."
    "TimeSinceLastActiveAdsIntervention";
const char kSubresourceFilterActionsHistogram[] = "SubresourceFilter.Actions2";

#if BUILDFLAG(IS_ANDROID)
class TestInfoBarManagerObserver : public infobars::InfoBarManager::Observer {
 public:
  TestInfoBarManagerObserver() = default;
  ~TestInfoBarManagerObserver() override = default;
  void OnInfoBarAdded(infobars::InfoBar* infobar) override {
    if (on_infobar_added_callback_)
      std::move(on_infobar_added_callback_).Run();
  }

  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override {
    if (on_infobar_removed_callback_)
      std::move(on_infobar_removed_callback_).Run();
  }

  void set_on_infobar_added_callback(base::OnceClosure callback) {
    on_infobar_added_callback_ = std::move(callback);
  }

  void set_on_infobar_removed_callback(base::OnceClosure callback) {
    on_infobar_removed_callback_ = std::move(callback);
  }

 private:
  base::OnceClosure on_infobar_added_callback_;
  base::OnceClosure on_infobar_removed_callback_;
};
#endif  // if BUILDFLAG(IS_ANDROID)

}  // namespace

// Tests that the ruleset service is available.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, RulesetService) {
  EXPECT_NE(BrowserProcess::GetInstance()->subresource_filter_ruleset_service(),
            nullptr);
}

// Tests that the expected ruleset data was published as part of startup.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, RulesArePublished) {
  auto* ruleset_service =
      BrowserProcess::GetInstance()->subresource_filter_ruleset_service();

  auto ruleset_version = ruleset_service->GetMostRecentlyIndexedVersion();
  EXPECT_TRUE(ruleset_version.IsValid());

  std::string most_recently_indexed_content_version =
      ruleset_version.content_version;

  std::string packaged_ruleset_manifest_string =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_SUBRESOURCE_FILTER_UNINDEXED_RULESET_MANIFEST_JSON);
  auto packaged_ruleset_manifest =
      base::JSONReader::Read(packaged_ruleset_manifest_string);
  std::string* packaged_content_version =
      packaged_ruleset_manifest->FindStringKey("version");

  EXPECT_EQ(most_recently_indexed_content_version, *packaged_content_version);
}

// The below test is restricted to Android as it tests activation of the
// subresource filter in its default production configuration and WebLayer
// currently has a safe browsing database available in production only on
// Android; the safe browsing database being non-null is a prerequisite for
// subresource filter operation.
#if BUILDFLAG(IS_ANDROID)

// Tests that page activation state is computed as part of a pageload.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PageActivationStateComputed) {
  // Set up prereqs.
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();

  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(subresource_filter::kActivationConsoleMessage);

  GURL test_url(embedded_test_server()->GetURL("/simple_page.html"));

  subresource_filter::TestSubresourceFilterObserver observer(web_contents);
  absl::optional<subresource_filter::mojom::ActivationLevel> page_activation =
      observer.GetPageActivation(test_url);
  EXPECT_FALSE(page_activation);

  // Verify that a navigation results in both (a) the page activation level
  // being computed, and (b) the result of that computation being the default
  // level of "dry run" due to AdTagging.
  NavigateAndWaitForCompletion(test_url, shell());

  page_activation = observer.GetPageActivation(test_url);

  EXPECT_TRUE(page_activation);
  EXPECT_EQ(subresource_filter::mojom::ActivationLevel::kDryRun,
            page_activation.value());

  EXPECT_TRUE(console_observer.messages().empty());
}

#endif  // (OS_ANDROID)

// Verifies that subframes that are flagged by the subresource filter ruleset
// are blocked from loading on activated URLs.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       DisallowedSubframeURLBlockedOnActivatedURL) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();

  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(subresource_filter::kActivationConsoleMessage);

  GURL test_url(embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_included_script.html"));

  subresource_filter::TestSubresourceFilterObserver observer(web_contents);
  absl::optional<subresource_filter::mojom::ActivationLevel> page_activation =
      observer.GetPageActivation(test_url);
  EXPECT_FALSE(page_activation);

  ActivateSubresourceFilterInWebContentsForURL(web_contents, test_url);

  // Verify that the "ad" subframe is loaded if it is not flagged by the
  // ruleset.
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));

  NavigateAndWaitForCompletion(test_url, shell());

  // The subresource filter should have been activated on this navigation...
  page_activation = observer.GetPageActivation(test_url);
  EXPECT_TRUE(page_activation);
  EXPECT_EQ(subresource_filter::mojom::ActivationLevel::kEnabled,
            page_activation.value());

  EXPECT_TRUE(console_observer.Wait());

  // ... but it should not have blocked the subframe from being loaded.
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // Do a different-document navigation to ensure that that the next navigation
  // to |test_url| executes as desired (e.g., to avoid any optimizations from
  // being made due to it being a same-document navigation that would interfere
  // with the logic of the test). Without this intervening navigation, we have
  // seen flake on the Windows trybot that indicates that such optimizations are
  // occurring.
  NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // Verify that the "ad" subframe is blocked if it is flagged by the
  // ruleset.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // Do a different-document navigation to ensure that that the next navigation
  // to |test_url| executes as desired (e.g., to avoid any optimizations from
  // being made due to it being a same-document navigation that would interfere
  // with the logic of the test). Without this intervening navigation, we have
  // seen flake on the Windows trybot that indicates that such optimizations are
  // occurring.
  NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // The main frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("frame_with_included_script.html");
  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
}

// Verifies that subframes are not blocked on non-activated URLs.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       DisallowedSubframeURLNotBlockedOnNonActivatedURL) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();

  GURL test_url(embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_included_script.html"));

  // Verify that the "ad" subframe is loaded if it is not flagged by the
  // ruleset.
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));

  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // Verify that the "ad" subframe is loaded if even it is flagged by the
  // ruleset as the URL is not activated.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ContentSettingsAllowlist_DoNotActivate) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();

  GURL test_url(embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_included_script.html"));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ActivateSubresourceFilterInWebContentsForURL(web_contents, test_url);

  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(subresource_filter::kActivationConsoleMessage);

  // Simulate explicitly allowlisting via content settings.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  settings_map->SetContentSettingDefaultScope(
      test_url, test_url, ContentSettingsType::ADS, CONTENT_SETTING_ALLOW);

  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // No message for allowlisted url.
  EXPECT_TRUE(console_observer.messages().empty());
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       DISABLED_ContentSettingsAllowlistGlobal_DoNotActivate) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();

  GURL test_url(embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_included_script.html"));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ActivateSubresourceFilterInWebContentsForURL(web_contents, test_url);

  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(subresource_filter::kActivationConsoleMessage);

  // Simulate globally allowing ads via content settings.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  settings_map->SetDefaultContentSetting(ContentSettingsType::ADS,
                                         CONTENT_SETTING_ALLOW);

  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // No message for loads that are not activated.
  EXPECT_TRUE(console_observer.messages().empty());
}

#if BUILDFLAG(IS_ANDROID)
// Test that the ads blocked infobar is presented when visiting a page where the
// subresource filter blocks resources from being loaded and is removed when
// navigating away.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, InfoBarPresentation) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();
  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);

  // Configure the subresource filter to activate on the test URL and to block
  // its script from loading.
  GURL test_url(embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_included_script.html"));
  ActivateSubresourceFilterInWebContentsForURL(web_contents, test_url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  TestInfoBarManagerObserver infobar_observer;
  infobar_manager->AddObserver(&infobar_observer);

  base::RunLoop run_loop;
  infobar_observer.set_on_infobar_added_callback(run_loop.QuitClosure());

  EXPECT_EQ(0u, infobar_manager->infobar_count());

  // Navigate such that the script is blocked and verify that the ads blocked
  // infobar is presented.
  NavigateAndWaitForCompletion(test_url, shell());
  run_loop.Run();

  EXPECT_EQ(1u, infobar_manager->infobar_count());
  auto* infobar =
      static_cast<infobars::InfoBarAndroid*>(infobar_manager->infobar_at(0));
  EXPECT_TRUE(infobar->HasSetJavaInfoBar());
  EXPECT_EQ(infobar->delegate()->GetIdentifier(),
            infobars::InfoBarDelegate::ADS_BLOCKED_INFOBAR_DELEGATE_ANDROID);

  // Navigate away and verify that the infobar is removed.
  base::RunLoop run_loop2;
  infobar_observer.set_on_infobar_removed_callback(run_loop2.QuitClosure());

  NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  run_loop2.Run();

  EXPECT_EQ(0u, infobar_manager->infobar_count());
  infobar_manager->RemoveObserver(&infobar_observer);
}
#endif

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ContentSettingsAllowlistViaReload_DoNotActivate) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL test_url(embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_included_script.html"));
  ActivateSubresourceFilterInWebContentsForURL(web_contents, test_url);

  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // Allowlist via a reload.
  content::TestNavigationObserver navigation_observer(web_contents, 1);
  GetPrimaryPageThrottleManager()->OnReloadRequested();
  navigation_observer.Wait();

  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       ContentSettingsAllowlistViaReload_AllowlistIsByDomain) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL test_url(embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_included_script.html"));
  ActivateSubresourceFilterInWebContentsForURL(web_contents, test_url);

  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // Allowlist via a reload.
  content::TestNavigationObserver navigation_observer(web_contents, 1);
  GetPrimaryPageThrottleManager()->OnReloadRequested();
  navigation_observer.Wait();

  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // Another navigation to the same domain should be allowed too.
  NavigateAndWaitForCompletion(
      embedded_test_server()->GetURL(
          "/subresource_filter/frame_with_included_script.html?query"),
      shell());
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  // A cross site blocklisted navigation should stay activated, however.
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/frame_with_included_script.html"));
  ActivateSubresourceFilterInWebContentsForURL(web_contents, a_url);
  NavigateAndWaitForCompletion(a_url, shell());
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       AdsInterventionEnforced_PageActivated) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto* ads_intervention_manager =
      SubresourceFilterProfileContextFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())
          ->ads_intervention_manager();
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  ads_intervention_manager->set_clock_for_testing(test_clock.get());

  const GURL url(embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_included_script.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  // Should not trigger activation as the URL is not on the blocklist and
  // has no active ads interventions.
  NavigateAndWaitForCompletion(url, shell());
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
  histogram_tester.ExpectTotalCount(kTimeSinceAdsInterventionTriggeredHistogram,
                                    0);
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Trigger an ads violation and renavigate the page. Should trigger
  // subresource filter activation.
  GetPrimaryPageThrottleManager()->OnAdsViolationTriggered(
      web_contents->GetPrimaryMainFrame(),
      subresource_filter::mojom::AdsViolation::kMobileAdDensityByHeightAbove30);
  NavigateAndWaitForCompletion(url, shell());

  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(subresource_filter::mojom::AdsViolation::
                           kMobileAdDensityByHeightAbove30),
      1);
  histogram_tester.ExpectBucketCount(
      kTimeSinceAdsInterventionTriggeredHistogram, 0, 1);
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdsIntervention_LastIntervention::kInterventionTypeName,
      static_cast<int>(subresource_filter::mojom::AdsViolation::
                           kMobileAdDensityByHeightAbove30));
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdsIntervention_LastIntervention::kInterventionStatusName,
      static_cast<int>(AdsInterventionStatus::kBlocking));

  // Advance the clock to clear the intervention.
  test_clock->Advance(subresource_filter::kAdsInterventionDuration.Get());
  NavigateAndWaitForCompletion(url, shell());

  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(subresource_filter::mojom::AdsViolation::
                           kMobileAdDensityByHeightAbove30),
      1);
  histogram_tester.ExpectBucketCount(
      kTimeSinceAdsInterventionTriggeredHistogram,
      subresource_filter::kAdsInterventionDuration.Get().InHours(), 1);
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(2u, entries.size());

  // One of the entries is kBlocking, verify that the other is kExpired after
  // the intervention is cleared.
  EXPECT_TRUE(
      (*ukm_recorder.GetEntryMetric(
           entries.front(), ukm::builders::AdsIntervention_LastIntervention::
                                kInterventionStatusName) ==
       static_cast<int>(AdsInterventionStatus::kExpired)) ||
      (*ukm_recorder.GetEntryMetric(
           entries.back(), ukm::builders::AdsIntervention_LastIntervention::
                               kInterventionStatusName) ==
       static_cast<int>(AdsInterventionStatus::kExpired)));
}

IN_PROC_BROWSER_TEST_F(
    SubresourceFilterBrowserTest,
    MultipleAdsInterventions_PageActivationClearedAfterFirst) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto* ads_intervention_manager =
      SubresourceFilterProfileContextFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())
          ->ads_intervention_manager();
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  ads_intervention_manager->set_clock_for_testing(test_clock.get());

  const GURL url(embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_included_script.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  // Should not trigger activation as the URL is not on the blocklist and
  // has no active ads interventions.
  NavigateAndWaitForCompletion(url, shell());
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
  histogram_tester.ExpectTotalCount(kAdsInterventionRecordedHistogram, 0);
  histogram_tester.ExpectTotalCount(kTimeSinceAdsInterventionTriggeredHistogram,
                                    0);
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Trigger an ads violation and renavigate the page. Should trigger
  // subresource filter activation.
  GetPrimaryPageThrottleManager()->OnAdsViolationTriggered(
      web_contents->GetPrimaryMainFrame(),
      subresource_filter::mojom::AdsViolation::kMobileAdDensityByHeightAbove30);
  NavigateAndWaitForCompletion(url, shell());

  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(subresource_filter::mojom::AdsViolation::
                           kMobileAdDensityByHeightAbove30),
      1);
  histogram_tester.ExpectBucketCount(
      kTimeSinceAdsInterventionTriggeredHistogram, 0, 1);
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdsIntervention_LastIntervention::kInterventionTypeName,
      static_cast<int>(subresource_filter::mojom::AdsViolation::
                           kMobileAdDensityByHeightAbove30));
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdsIntervention_LastIntervention::kInterventionStatusName,
      static_cast<int>(AdsInterventionStatus::kBlocking));

  // Advance the clock by less than kAdsInterventionDuration and trigger another
  // intervention. This intervention is a no-op.
  test_clock->Advance(subresource_filter::kAdsInterventionDuration.Get() -
                      base::Minutes(30));
  GetPrimaryPageThrottleManager()->OnAdsViolationTriggered(
      web_contents->GetPrimaryMainFrame(),
      subresource_filter::mojom::AdsViolation::kMobileAdDensityByHeightAbove30);

  // Advance the clock to to kAdsInterventionDuration from the first
  // intervention, this clear the intervention.
  test_clock->Advance(base::Minutes(30));
  NavigateAndWaitForCompletion(url, shell());

  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(subresource_filter::mojom::AdsViolation::
                           kMobileAdDensityByHeightAbove30),
      1);
  histogram_tester.ExpectBucketCount(
      kTimeSinceAdsInterventionTriggeredHistogram,
      subresource_filter::kAdsInterventionDuration.Get().InHours(), 1);
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(2u, entries.size());

  // One of the entries is kBlocking, verify that the other is kExpired after
  // the intervention is cleared.
  EXPECT_TRUE(
      (*ukm_recorder.GetEntryMetric(
           entries.front(), ukm::builders::AdsIntervention_LastIntervention::
                                kInterventionStatusName) ==
       static_cast<int>(AdsInterventionStatus::kExpired)) ||
      (*ukm_recorder.GetEntryMetric(
           entries.back(), ukm::builders::AdsIntervention_LastIntervention::
                               kInterventionStatusName) ==
       static_cast<int>(AdsInterventionStatus::kExpired)));
}

class SubresourceFilterBrowserTestWithoutAdsInterventionEnforcement
    : public SubresourceFilterBrowserTest {
 public:
  SubresourceFilterBrowserTestWithoutAdsInterventionEnforcement() {
    feature_list_.InitAndDisableFeature(
        subresource_filter::kAdsInterventionsEnforced);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    SubresourceFilterBrowserTestWithoutAdsInterventionEnforcement,
    AdsInterventionNotEnforced_NoPageActivation) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  auto* ads_intervention_manager =
      SubresourceFilterProfileContextFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())
          ->ads_intervention_manager();
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  ads_intervention_manager->set_clock_for_testing(test_clock.get());

  const GURL url(embedded_test_server()->GetURL(
      "/subresource_filter/frame_with_included_script.html"));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  // Should not trigger activation as the URL is not on the blocklist and
  // has no active ads interventions.
  NavigateAndWaitForCompletion(url, shell());
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Trigger an ads violation and renavigate to the page. Interventions are not
  // enforced so no activation should occur.
  GetPrimaryPageThrottleManager()->OnAdsViolationTriggered(
      web_contents->GetPrimaryMainFrame(),
      subresource_filter::mojom::AdsViolation::kMobileAdDensityByHeightAbove30);

  const base::TimeDelta kRenavigationDelay = base::Hours(2);
  test_clock->Advance(kRenavigationDelay);
  NavigateAndWaitForCompletion(url, shell());

  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));
  histogram_tester.ExpectBucketCount(
      kAdsInterventionRecordedHistogram,
      static_cast<int>(subresource_filter::mojom::AdsViolation::
                           kMobileAdDensityByHeightAbove30),
      1);
  histogram_tester.ExpectBucketCount(
      kTimeSinceAdsInterventionTriggeredHistogram, kRenavigationDelay.InHours(),
      1);
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::AdsIntervention_LastIntervention::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdsIntervention_LastIntervention::kInterventionTypeName,
      static_cast<int>(subresource_filter::mojom::AdsViolation::
                           kMobileAdDensityByHeightAbove30));
  ukm_recorder.ExpectEntryMetric(
      entries.front(),
      ukm::builders::AdsIntervention_LastIntervention::kInterventionStatusName,
      static_cast<int>(AdsInterventionStatus::kWouldBlock));
}

// Test the "smart" UI, aka the logic to hide the UI on subsequent same-domain
// navigations, until a certain time threshold has been reached. This is an
// android-only feature.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       DoNotShowUIUntilThresholdReached) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();
  auto* settings_manager =
      SubresourceFilterProfileContextFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())
          ->settings_manager();
  settings_manager->set_should_use_smart_ui_for_testing(true);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  GURL a_url(embedded_test_server()->GetURL(
      "a.com", "/subresource_filter/frame_with_included_script.html"));
  GURL b_url(embedded_test_server()->GetURL(
      "b.com", "/subresource_filter/frame_with_included_script.html"));
  // Test utils only support one blocklisted site at a time.
  // TODO(csharrison): Add support for more than one URL.
  ActivateSubresourceFilterInWebContentsForURL(web_contents, a_url);

  auto test_clock = std::make_unique<base::SimpleTestClock>();
  base::SimpleTestClock* raw_clock = test_clock.get();
  settings_manager->set_clock_for_testing(std::move(test_clock));

  base::HistogramTester histogram_tester;

  // First load should trigger the UI.
  NavigateAndWaitForCompletion(a_url, shell());
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 1);
  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUISuppressed, 0);

  // Second load should not trigger the UI, but should still filter content.
  NavigateAndWaitForCompletion(a_url, shell());
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 1);
  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUISuppressed, 1);

  ActivateSubresourceFilterInWebContentsForURL(web_contents, b_url);

  // Load to another domain should trigger the UI.
  NavigateAndWaitForCompletion(b_url, shell());
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 2);

  ActivateSubresourceFilterInWebContentsForURL(web_contents, a_url);

  // Fast forward the clock, and a_url should trigger the UI again.
  raw_clock->Advance(
      subresource_filter::SubresourceFilterContentSettingsManager::
          kDelayBeforeShowingInfobarAgain);
  NavigateAndWaitForCompletion(a_url, shell());
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents->GetPrimaryMainFrame()));

  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUIShown, 3);
  histogram_tester.ExpectBucketCount(
      kSubresourceFilterActionsHistogram,
      subresource_filter::SubresourceFilterAction::kUISuppressed, 1);
}

}  // namespace weblayer
