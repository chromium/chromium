// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_TEST_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_
#define WEBLAYER_TEST_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_

#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "weblayer/test/weblayer_browser_test.h"

namespace content {
class WebContents;
}

namespace weblayer {

// A base class for //weblayer browsertests that directly test or rely on
// subresource filter functionality.
class SubresourceFilterBrowserTest : public WebLayerBrowserTest {
 public:
  SubresourceFilterBrowserTest();
  ~SubresourceFilterBrowserTest() override;

  SubresourceFilterBrowserTest(const SubresourceFilterBrowserTest&) = delete;
  SubresourceFilterBrowserTest& operator=(const SubresourceFilterBrowserTest&) =
      delete;

  void SetUpOnMainThread() override;

 protected:
  void SetRulesetToDisallowURLsWithPathSuffix(const std::string& suffix);
  void SetRulesetWithRules(
      const std::vector<url_pattern_index::proto::UrlRule>& rules);

  content::WebContents* web_contents();

#if !defined(OS_ANDROID)
  // Installs a fake database manager so that the safe browsing activation
  // throttle will be created (WebLayer currently has a safe browsing database
  // available in production only on Android).
  void InstallFakeSafeBrowsingDatabaseManagerInWebContents(
      content::WebContents* web_contents);
#endif

  // By default SubresourceFilterBrowsertest starts the embedded test server in
  // SetUpOnMainThread(). Tests that wish to control the starting of the
  // embedded test server themselves should override this method to return
  // false.
  virtual bool StartEmbeddedTestServerAutomatically();

 private:
  subresource_filter::testing::TestRulesetCreator test_ruleset_creator_;
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace weblayer

#endif  // WEBLAYER_TEST_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_
