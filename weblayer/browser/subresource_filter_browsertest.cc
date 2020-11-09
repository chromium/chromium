// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/grit/weblayer_resources.h"
#include "weblayer/test/weblayer_browser_test.h"

namespace weblayer {

class SubresourceFilterBrowserTest : public WebLayerBrowserTest {
 public:
  SubresourceFilterBrowserTest() = default;
  ~SubresourceFilterBrowserTest() override = default;
  SubresourceFilterBrowserTest(const SubresourceFilterBrowserTest&) = delete;
  SubresourceFilterBrowserTest& operator=(const SubresourceFilterBrowserTest&) =
      delete;
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

}  // namespace weblayer
