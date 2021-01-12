// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "build/build_config.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/fake_safe_browsing_database_manager.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/subresource_filter_client_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/grit/weblayer_resources.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

#if defined(OS_ANDROID)
#include "components/infobars/android/infobar_android.h"  // nogncheck
#include "components/infobars/core/infobar_manager.h"     // nogncheck
#include "weblayer/browser/infobar_service.h"
#endif

namespace weblayer {

namespace {

// Returns whether a script resource that sets document.scriptExecuted to true
// on load was loaded.
bool WasParsedScriptElementLoaded(content::RenderFrameHost* rfh) {
  DCHECK(rfh);
  bool script_resource_was_loaded = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      rfh, "domAutomationController.send(!!document.scriptExecuted)",
      &script_resource_was_loaded));
  return script_resource_was_loaded;
}

#if defined(OS_ANDROID)
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
#endif  // if defined(OS_ANDROID)

}  // namespace

class SubresourceFilterBrowserTest : public WebLayerBrowserTest {
 public:
  SubresourceFilterBrowserTest() = default;
  ~SubresourceFilterBrowserTest() override = default;
  SubresourceFilterBrowserTest(const SubresourceFilterBrowserTest&) = delete;
  SubresourceFilterBrowserTest& operator=(const SubresourceFilterBrowserTest&) =
      delete;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void SetRulesetToDisallowURLsWithPathSuffix(const std::string& suffix) {
    subresource_filter::testing::TestRulesetPair test_ruleset_pair;
    subresource_filter::testing::TestRulesetCreator test_ruleset_creator;
    test_ruleset_creator.CreateRulesetToDisallowURLsWithPathSuffix(
        suffix, &test_ruleset_pair);

    subresource_filter::testing::TestRulesetPublisher test_ruleset_publisher(
        BrowserProcess::GetInstance()->subresource_filter_ruleset_service());
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));
  }

  // Configures the database manager to activate on |url| in |web_contents|.
  void ActivateSubresourceFilterInWebContentsForURL(
      content::WebContents* web_contents,
      const GURL& url) {
    scoped_refptr<FakeSafeBrowsingDatabaseManager> database_manager =
        base::MakeRefCounted<FakeSafeBrowsingDatabaseManager>();
    database_manager->AddBlocklistedUrl(
        url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING);

    auto* client_impl = static_cast<SubresourceFilterClientImpl*>(
        subresource_filter::ContentSubresourceFilterThrottleManager::
            FromWebContents(web_contents)
                ->client());
    client_impl->set_database_manager_for_testing(std::move(database_manager));
  }
};

// Tests that the ruleset service is available.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, RulesetService) {
  EXPECT_NE(BrowserProcess::GetInstance()->subresource_filter_ruleset_service(),
            nullptr);
}

// Tests that the ruleset is published as part of startup.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, RulesArePublished) {
  auto* ruleset_service =
      BrowserProcess::GetInstance()->subresource_filter_ruleset_service();

  // Publishing might or might not have already finished at this point; wait for
  // it to finish if necessary.
  if (!ruleset_service->GetMostRecentlyIndexedVersion().IsValid()) {
    base::RunLoop run_loop;
    ruleset_service->SetRulesetPublishedCallbackForTesting(
        run_loop.QuitClosure());

    run_loop.Run();
  }

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
#if defined(OS_ANDROID)

// Tests that page activation state is computed as part of a pageload.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       PageActivationStateComputed) {
  // Set up prereqs.
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();

  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(subresource_filter::kActivationConsoleMessage);

  GURL test_url(embedded_test_server()->GetURL("/simple_page.html"));

  subresource_filter::TestSubresourceFilterObserver observer(web_contents);
  base::Optional<subresource_filter::mojom::ActivationLevel> page_activation =
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
// Flaky on Windows. See https://crbug.com/1152429
#if defined(OS_WIN)
#define MAYBE_DisallowedSubframeURLBlockedOnActivatedURL \
  DISABLED_DisallowedSubframeURLBlockedOnActivatedURL
#else
#define MAYBE_DisallowedSubframeURLBlockedOnActivatedURL \
  DisallowedSubframeURLBlockedOnActivatedURL
#endif
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       MAYBE_DisallowedSubframeURLBlockedOnActivatedURL) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();

  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(subresource_filter::kActivationConsoleMessage);

  GURL test_url(
      embedded_test_server()->GetURL("/frame_with_included_script.html"));

  subresource_filter::TestSubresourceFilterObserver observer(web_contents);
  base::Optional<subresource_filter::mojom::ActivationLevel> page_activation =
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
  EXPECT_FALSE(console_observer.messages().empty());

  // ... but it should not have blocked the subframe from being loaded.
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents->GetMainFrame()));

  // Do a different-document navigation to ensure that that the next navigation
  // to |test_url| executes as desired (e.g., to avoid any optimizations from
  // being made due to it being a same-document navigation that would interfere
  // with the logic of the test). Without this intervening navigation, we have
  // seen flake on the Windows trybot that indicates that such optimizations are
  // occurring.
  NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents->GetMainFrame()));

  // Verify that the "ad" subframe is blocked if it is flagged by the
  // ruleset.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents->GetMainFrame()));

  // Do a different-document navigation to ensure that that the next navigation
  // to |test_url| executes as desired (e.g., to avoid any optimizations from
  // being made due to it being a same-document navigation that would interfere
  // with the logic of the test). Without this intervening navigation, we have
  // seen flake on the Windows trybot that indicates that such optimizations are
  // occurring.
  NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  EXPECT_FALSE(WasParsedScriptElementLoaded(web_contents->GetMainFrame()));

  // The main frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("frame_with_included_script.html");
  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents->GetMainFrame()));
}

// Verifies that subframes are not blocked on non-activated URLs.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       DisallowedSubframeURLNotBlockedOnNonActivatedURL) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();

  GURL test_url(
      embedded_test_server()->GetURL("/frame_with_included_script.html"));

  // Verify that the "ad" subframe is loaded if it is not flagged by the
  // ruleset.
  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithPathSuffix(
      "suffix-that-does-not-match-anything"));

  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents->GetMainFrame()));

  // Verify that the "ad" subframe is loaded if even it is flagged by the
  // ruleset as the URL is not activated.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_TRUE(WasParsedScriptElementLoaded(web_contents->GetMainFrame()));
}

#if defined(OS_ANDROID)
// Test that the ads blocked infobar is presented when visiting a page where the
// subresource filter blocks resources from being loaded and is removed when
// navigating away.
IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, InfoBarPresentation) {
  auto* web_contents = static_cast<TabImpl*>(shell()->tab())->web_contents();
  auto* infobar_service = InfoBarService::FromWebContents(web_contents);

  // Configure the subresource filter to activate on the test URL and to block
  // its script from loading.
  GURL test_url(
      embedded_test_server()->GetURL("/frame_with_included_script.html"));
  ActivateSubresourceFilterInWebContentsForURL(web_contents, test_url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));

  TestInfoBarManagerObserver infobar_observer;
  infobar_service->AddObserver(&infobar_observer);

  base::RunLoop run_loop;
  infobar_observer.set_on_infobar_added_callback(run_loop.QuitClosure());

  EXPECT_EQ(0u, infobar_service->infobar_count());

  // Navigate such that the script is blocked and verify that the ads blocked
  // infobar is presented.
  NavigateAndWaitForCompletion(test_url, shell());
  run_loop.Run();

  EXPECT_EQ(1u, infobar_service->infobar_count());
  auto* infobar =
      static_cast<infobars::InfoBarAndroid*>(infobar_service->infobar_at(0));
  EXPECT_TRUE(infobar->HasSetJavaInfoBar());
  EXPECT_EQ(infobar->delegate()->GetIdentifier(),
            infobars::InfoBarDelegate::ADS_BLOCKED_INFOBAR_DELEGATE_ANDROID);

  // Navigate away and verify that the infobar is removed.
  base::RunLoop run_loop2;
  infobar_observer.set_on_infobar_removed_callback(run_loop2.QuitClosure());

  NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  run_loop2.Run();

  EXPECT_EQ(0u, infobar_service->infobar_count());
  infobar_service->RemoveObserver(&infobar_observer);
}
#endif

}  // namespace weblayer
