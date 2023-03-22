// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_page_focus_override.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class ClipboardBrowserTest : public WebLayerBrowserTest {
 public:
  ClipboardBrowserTest() = default;
  ~ClipboardBrowserTest() override = default;

  // WebLayerBrowserTest:
  void SetUpOnMainThread() override {
    WebLayerBrowserTest::SetUpOnMainThread();
    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(
            GetWebContents());
    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);

    title_watcher_ =
        std::make_unique<content::TitleWatcher>(GetWebContents(), u"success");
    title_watcher_->AlsoWaitForTitle(u"fail");

    EXPECT_TRUE(embedded_test_server()->Start());
    NavigateAndWaitForCompletion(
        embedded_test_server()->GetURL("/clipboard.html"), shell());

    // The Clipboard API requires the page to have focus.
    scoped_focus_ =
        std::make_unique<content::ScopedPageFocusOverride>(GetWebContents());
  }

  void TearDownOnMainThread() override {
    scoped_focus_.reset();
    title_watcher_.reset();
    prompt_factory_.reset();
  }

 protected:
  content::WebContents* GetWebContents() {
    return static_cast<TabImpl*>(shell()->tab())->web_contents();
  }

  GURL GetBaseOrigin() {
    return embedded_test_server()->base_url().DeprecatedGetOriginAsURL();
  }

  permissions::MockPermissionPromptFactory* prompt_factory() {
    return prompt_factory_.get();
  }

  content::TitleWatcher* title_watcher() { return title_watcher_.get(); }

 private:
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
  std::unique_ptr<content::TitleWatcher> title_watcher_;
  std::unique_ptr<content::ScopedPageFocusOverride> scoped_focus_;
};

IN_PROC_BROWSER_TEST_F(ClipboardBrowserTest, ReadTextSuccess) {
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  ExecuteScriptWithUserGesture(shell()->tab(), "tryClipboardReadText()");
  EXPECT_EQ(u"success", title_watcher()->WaitAndGetTitle());

  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
  EXPECT_TRUE(prompt_factory()->RequestOriginSeen(GetBaseOrigin()));
}

IN_PROC_BROWSER_TEST_F(ClipboardBrowserTest, WriteSanitizedTextSuccess) {
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  ExecuteScriptWithUserGesture(shell()->tab(), "tryClipboardWriteText()");
  EXPECT_EQ(u"success", title_watcher()->WaitAndGetTitle());

  // Writing sanitized data to the clipboard does not require a permission.
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());
}

IN_PROC_BROWSER_TEST_F(ClipboardBrowserTest, ReadTextWithoutPermission) {
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DENY_ALL);
  ExecuteScriptWithUserGesture(shell()->tab(), "tryClipboardReadText()");
  EXPECT_EQ(u"fail", title_watcher()->WaitAndGetTitle());

  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
  EXPECT_TRUE(prompt_factory()->RequestOriginSeen(GetBaseOrigin()));
}

}  // namespace weblayer
