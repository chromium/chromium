// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "weblayer/browser/browser_process.h"
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

}  // namespace weblayer
