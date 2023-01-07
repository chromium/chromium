// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/permissions_client.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class PermissionsBrowserTest : public WebLayerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  content::WebContents* GetWebContents() {
    Tab* tab = shell()->tab();
    TabImpl* tab_impl = static_cast<TabImpl*>(tab);
    return tab_impl->web_contents();
  }

  GURL GetCurrentDisplayURL() {
    auto* navigation_controller = shell()->tab()->GetNavigationController();
    return navigation_controller->GetNavigationEntryDisplayURL(
        navigation_controller->GetNavigationListCurrentIndex());
  }
};

IN_PROC_BROWSER_TEST_F(PermissionsBrowserTest, SubresourceFilterActivation) {
  NavigateAndWaitForCompletion(GURL("about:blank"), shell());
  EXPECT_FALSE(
      permissions::PermissionsClient::Get()->IsSubresourceFilterActivated(
          GetWebContents()->GetBrowserContext(), GetCurrentDisplayURL()));

  GURL test_url(embedded_test_server()->GetURL("/simple_page.html"));
  ActivateSubresourceFilterInWebContentsForURL(GetWebContents(), test_url);

  NavigateAndWaitForCompletion(test_url, shell());
  EXPECT_TRUE(
      permissions::PermissionsClient::Get()->IsSubresourceFilterActivated(
          GetWebContents()->GetBrowserContext(), GetCurrentDisplayURL()));
}

}  // namespace weblayer
