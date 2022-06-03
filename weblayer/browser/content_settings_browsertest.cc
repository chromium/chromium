// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/cookie_settings.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/cookie_settings_factory.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {
namespace {
constexpr char kHasLocalStorageScript[] = R"(
  new Promise(function (resolve, reject) {
    try {
      localStorage.setItem('foo', 'bar');
      resolve(true);
    } catch(e) {
      resolve(false);
    }
  })
)";
}  // namespace

using ContentSettingsBrowserTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(ContentSettingsBrowserTest, LocalStorageAvailable) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateAndWaitForCompletion(embedded_test_server()->GetURL("/echo"),
                               shell());
  content::WebContents* web_contents =
      static_cast<TabImpl*>(shell()->tab())->web_contents();
  EXPECT_TRUE(
      content::EvalJs(web_contents, kHasLocalStorageScript).ExtractBool());
}

IN_PROC_BROWSER_TEST_F(ContentSettingsBrowserTest, LocalStorageDenied) {
  ASSERT_TRUE(embedded_test_server()->Start());
  NavigateAndWaitForCompletion(embedded_test_server()->GetURL("/echo"),
                               shell());
  content::WebContents* web_contents =
      static_cast<TabImpl*>(shell()->tab())->web_contents();
  // Blocking cookies for this URL should also block local storage.
  CookieSettingsFactory::GetForBrowserContext(web_contents->GetBrowserContext())
      ->SetCookieSetting(embedded_test_server()->base_url(),
                         CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(
      content::EvalJs(web_contents, kHasLocalStorageScript).ExtractBool());
}

}  // namespace weblayer
