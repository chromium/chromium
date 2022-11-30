// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/subresource_filter_browser_test_harness.h"

#include "build/build_config.h"
#include "components/heavy_ad_intervention/heavy_ad_service.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_web_contents_helper.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/heavy_ad_service_factory.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

// Waits for the subresource filter ruleset data to be published as part of
// WebLayer startup. Returns immediately if ruleset data has already been
// published.
void WaitForSubresourceFilterRulesetDataToBePublished() {
  auto* ruleset_service =
      BrowserProcess::GetInstance()->subresource_filter_ruleset_service();

  if (!ruleset_service->GetMostRecentlyIndexedVersion().IsValid()) {
    base::RunLoop run_loop;
    ruleset_service->SetRulesetPublishedCallbackForTesting(
        run_loop.QuitClosure());

    run_loop.Run();
  }
}

// Waits for the heavy ad blocklist data to be loaded as part of
// WebLayer startup.
void WaitForHeavyAdBlocklistToBeLoaded(content::WebContents* web_contents) {
  auto* heavy_ad_service = HeavyAdServiceFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());

  base::RunLoop run_loop;
  heavy_ad_service->NotifyOnBlocklistLoaded(run_loop.QuitClosure());
  run_loop.Run();
}

#if !BUILDFLAG(IS_ANDROID)
// Installs a fake database manager so that the safe browsing activation
// throttle will be created (WebLayer currently has a safe browsing database
// available in production only on Android).
void InstallFakeSafeBrowsingDatabaseManagerInWebContents(
    content::WebContents* web_contents) {
  scoped_refptr<FakeSafeBrowsingDatabaseManager> database_manager =
      base::MakeRefCounted<FakeSafeBrowsingDatabaseManager>();

  subresource_filter::ContentSubresourceFilterWebContentsHelper::
      FromWebContents(web_contents)
          ->SetDatabaseManagerForTesting(std::move(database_manager));
}
#endif

}  // namespace

SubresourceFilterBrowserTest::SubresourceFilterBrowserTest() {
  feature_list_.InitAndEnableFeature(
      subresource_filter::kAdsInterventionsEnforced);
}

SubresourceFilterBrowserTest::~SubresourceFilterBrowserTest() = default;

void SubresourceFilterBrowserTest::SetUpOnMainThread() {
  // Wait for the initial publishing of production data that occurs as part of
  // startup to complete. This is crucial for tests that inject test ruleset
  // data and wait for it to be published via TestRulesetPublisher: if the
  // initial publishing is still in process when those tests start running,
  // they can end up incorrectly proceeding on the publishing of the
  // production data rather than their test data.
  WaitForSubresourceFilterRulesetDataToBePublished();

  // Wait for the heavy ad blocklist loading that occurs as part of ProfileImpl
  // creation to complete. This is crucial for tests of heavy ad interventions:
  // if the blocklist isn't loaded all hosts are treated as blocklisted, which
  // interferes with the operation of those tests.
  WaitForHeavyAdBlocklistToBeLoaded(web_contents());

#if !BUILDFLAG(IS_ANDROID)
  // Install a fake database manager so that the safe browsing activation
  // throttle will be created, as that throttle is a core requirement for the
  // operation of the subresource filter.
  InstallFakeSafeBrowsingDatabaseManagerInWebContents(web_contents());
#endif

  embedded_test_server()->ServeFilesFromSourceDirectory("components/test/data");

  // This test suite does "cross-site" navigations to various domains that
  // must all resolve to localhost.
  host_resolver()->AddRule("*", "127.0.0.1");

  if (StartEmbeddedTestServerAutomatically())
    ASSERT_TRUE(embedded_test_server()->Start());
}

void SubresourceFilterBrowserTest::SetRulesetToDisallowURLsWithPathSuffix(
    const std::string& suffix) {
  subresource_filter::testing::TestRulesetPair test_ruleset_pair;
  test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
      suffix, &test_ruleset_pair);

  subresource_filter::testing::TestRulesetPublisher test_ruleset_publisher(
      BrowserProcess::GetInstance()->subresource_filter_ruleset_service());
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));
}

void SubresourceFilterBrowserTest::SetRulesetWithRules(
    const std::vector<url_pattern_index::proto::UrlRule>& rules) {
  subresource_filter::testing::TestRulesetPair test_ruleset_pair;
  test_ruleset_creator_.CreateRulesetWithRules(rules, &test_ruleset_pair);

  subresource_filter::testing::TestRulesetPublisher test_ruleset_publisher(
      BrowserProcess::GetInstance()->subresource_filter_ruleset_service());
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));
}

content::WebContents* SubresourceFilterBrowserTest::web_contents() {
  return static_cast<TabImpl*>(shell()->tab())->web_contents();
}

bool SubresourceFilterBrowserTest::WasParsedScriptElementLoaded(
    content::RenderFrameHost* rfh) {
  DCHECK(rfh);
  bool script_resource_was_loaded = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      rfh, "domAutomationController.send(!!document.scriptExecuted)",
      &script_resource_was_loaded));
  return script_resource_was_loaded;
}

bool SubresourceFilterBrowserTest::StartEmbeddedTestServerAutomatically() {
  return true;
}

subresource_filter::ContentSubresourceFilterThrottleManager*
SubresourceFilterBrowserTest::GetPrimaryPageThrottleManager() {
  return subresource_filter::ContentSubresourceFilterThrottleManager::FromPage(
      web_contents()->GetPrimaryPage());
}

}  // namespace weblayer
