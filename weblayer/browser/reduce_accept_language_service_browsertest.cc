// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/reduce_accept_language/browser/reduce_accept_language_service.h"
#include "components/reduce_accept_language/browser/reduce_accept_language_service_test_util.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/reduce_accept_language_factory.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

using reduce_accept_language::test::ReduceAcceptLanguageServiceTester;

namespace weblayer {

class ReduceAcceptLanguageServiceTest : public WebLayerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    WebLayerBrowserTest::SetUpOnMainThread();
    service_tester_ = std::make_unique<ReduceAcceptLanguageServiceTester>(
        settings_map(), service(), prefs());
    language::LanguagePrefs(prefs()).SetUserSelectedLanguagesList(
        {"en", "ja", "it"});
  }

  content::WebContents* web_contents() {
    return static_cast<TabImpl*>(shell()->tab())->web_contents();
  }

  HostContentSettingsMap* settings_map() {
    return HostContentSettingsMapFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext());
  }

  PrefService* prefs() {
    return static_cast<BrowserContextImpl*>(web_contents()->GetBrowserContext())
        ->pref_service();
  }

  reduce_accept_language::ReduceAcceptLanguageService* service() {
    return ReduceAcceptLanguageFactory::GetForBrowserContext(
        web_contents()->GetBrowserContext());
  }

  ReduceAcceptLanguageServiceTester* tester() { return service_tester_.get(); }

 private:
  std::unique_ptr<ReduceAcceptLanguageServiceTester> service_tester_;
};

IN_PROC_BROWSER_TEST_F(ReduceAcceptLanguageServiceTest, GetAcceptLanguageList) {
  tester()->VerifyFetchAcceptLanguageList({"en", "ja", "it"});
  reduce_accept_language::ReduceAcceptLanguageService incognito_service(
      settings_map(), prefs(), true);
  // Verify incognito mode only has first accept language.
  EXPECT_EQ(std::vector<std::string>{"en"},
            incognito_service.GetUserAcceptLanguages());
}

IN_PROC_BROWSER_TEST_F(ReduceAcceptLanguageServiceTest, PersistLanguageFail) {
  tester()->VerifyPersistFail(GURL("ws://example.com/"), "Zh-CN");
}

IN_PROC_BROWSER_TEST_F(ReduceAcceptLanguageServiceTest,
                       PersistLanguageSuccessJavaScriptNotEnabled) {
  tester()->VerifyPersistSuccessOnJavaScriptDisable(
      GURL("https://example.com/"), "Zh-CN");
}

IN_PROC_BROWSER_TEST_F(ReduceAcceptLanguageServiceTest,
                       PersistLanguageSuccess) {
  tester()->VerifyPersistSuccess(GURL("https://example.com/"), "Zh-CN");
}

IN_PROC_BROWSER_TEST_F(ReduceAcceptLanguageServiceTest,
                       PersistLanguageMultipleHosts) {
  tester()->VerifyPersistMultipleHostsSuccess(
      {GURL("https://example1.com/"), GURL("https://example2.com/"),
       GURL("http://example.com/")},
      {"en-US", "es-MX", "zh-CN"});
}

}  // namespace weblayer
