// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/site_isolation/features.h"
#include "components/site_isolation/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/content_browser_client_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {
using testing::IsEmpty;
using testing::UnorderedElementsAre;

class SiteIsolationBrowserTest : public WebLayerBrowserTest {
 public:
  SiteIsolationBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{site_isolation::features::kSiteIsolationMemoryThresholds,
          {{site_isolation::features::
                kPartialSiteIsolationMemoryThresholdParamName,
            "128"}}},
         {site_isolation::features::kSiteIsolationForPasswordSites, {}}},
        {});
  }

  std::vector<std::string> GetSavedIsolatedSites() {
    PrefService* prefs =
        user_prefs::UserPrefs::Get(GetProfile()->GetBrowserContext());
    const auto& list =
        prefs->GetList(site_isolation::prefs::kUserTriggeredIsolatedOrigins);
    std::vector<std::string> sites;
    for (const base::Value& value : list)
      sites.push_back(value.GetString());
    return sites;
  }

  // WebLayerBrowserTest:
  void SetUpOnMainThread() override {
    original_client_ = content::SetBrowserClientForTesting(&browser_client_);
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("weblayer/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    ASSERT_FALSE(
        content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites());
    ASSERT_TRUE(
        content::SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled());
  }

  void TearDownOnMainThread() override {
    content::SetBrowserClientForTesting(original_client_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("ignore-certificate-errors");

    // This way the test always sees the same amount of physical memory
    // (kLowMemoryDeviceThresholdMB = 512MB), regardless of how much memory is
    // available in the testing environment.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableLowEndDeviceMode);
    EXPECT_EQ(512, base::SysInfo::AmountOfPhysicalMemoryMB());
  }

  content::WebContents* GetWebContents() {
    return static_cast<TabImpl*>(shell()->tab())->web_contents();
  }

  void StartIsolatingSite(const GURL& url) {
    content::SiteInstance::StartIsolatingSite(
        GetProfile()->GetBrowserContext(), url,
        content::ChildProcessSecurityPolicy::IsolatedOriginSource::
            USER_TRIGGERED);
  }

 private:
  // A browser client which forces off strict site isolation, so the test can
  // assume password isolation is enabled.
  class SiteIsolationContentBrowserClient : public ContentBrowserClientImpl {
   public:
    SiteIsolationContentBrowserClient() : ContentBrowserClientImpl(nullptr) {}

    bool ShouldEnableStrictSiteIsolation() override { return false; }
  };

  SiteIsolationContentBrowserClient browser_client_;
  raw_ptr<content::ContentBrowserClient> original_client_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SiteIsolationBrowserTest,
                       SiteIsIsolatedAfterEnteringPassword) {
  GURL url = embedded_test_server()->GetURL("sub.foo.com",
                                            "/simple_password_form.html");
  NavigateAndWaitForCompletion(url, shell());
  content::WebContents* contents = GetWebContents();

  // foo.com should not be isolated to start with. Verify that a cross-site
  // iframe does not become an OOPIF.
  EXPECT_FALSE(contents->GetPrimaryMainFrame()
                   ->GetSiteInstance()
                   ->RequiresDedicatedProcess());
  std::string kAppendIframe = R"(
      var i = document.createElement('iframe');
      i.id = 'child';
      document.body.appendChild(i);)";
  EXPECT_TRUE(content::ExecJs(contents, kAppendIframe));
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/simple_page.html"));
  EXPECT_TRUE(NavigateIframeToURL(contents, "child", bar_url));
  content::RenderFrameHost* child =
      ChildFrameAt(contents->GetPrimaryMainFrame(), 0);
  EXPECT_FALSE(child->IsCrossProcessSubframe());

  // Fill a form and submit through a <input type="submit"> button.
  content::TestNavigationObserver observer(contents);
  std::string kFillAndSubmit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  EXPECT_TRUE(content::ExecJs(contents, kFillAndSubmit));
  observer.Wait();

  // Since there were no script references from other windows, we should've
  // swapped BrowsingInstances and put the result of the form submission into a
  // dedicated process, locked to foo.com.  Check that a cross-site iframe now
  // becomes an OOPIF.
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());
  EXPECT_TRUE(ExecJs(contents, kAppendIframe));
  EXPECT_TRUE(NavigateIframeToURL(contents, "child", bar_url));
  child = ChildFrameAt(contents->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(child->IsCrossProcessSubframe());
}

// TODO(crbug.com/654704): Android does not support PRE_ tests.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(SiteIsolationBrowserTest,
                       PRE_IsolatedSitesPersistAcrossRestarts) {
  // There shouldn't be any saved isolated origins to start with.
  EXPECT_THAT(GetSavedIsolatedSites(), IsEmpty());

  // Isolate saved.com and saved2.com persistently.
  GURL saved_url =
      embedded_test_server()->GetURL("saved.com", "/simple_page.html");
  StartIsolatingSite(saved_url);
  GURL saved2_url =
      embedded_test_server()->GetURL("saved2.com", "/simple_page.html");
  StartIsolatingSite(saved2_url);

  NavigateAndWaitForCompletion(saved_url, shell());
  EXPECT_TRUE(GetWebContents()
                  ->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());

  // Check that saved.com and saved2.com were saved to disk.
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("http://saved.com", "http://saved2.com"));
}

IN_PROC_BROWSER_TEST_F(SiteIsolationBrowserTest,
                       IsolatedSitesPersistAcrossRestarts) {
  // Check that saved.com and saved2.com are still saved to disk.
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("http://saved.com", "http://saved2.com"));

  // Check that these sites utilize a dedicated process after restarting, but a
  // non-isolated foo.com URL does not.
  GURL saved_url =
      embedded_test_server()->GetURL("saved.com", "/simple_page.html");
  GURL saved2_url =
      embedded_test_server()->GetURL("saved2.com", "/simple_page2.html");
  GURL foo_url =
      embedded_test_server()->GetURL("foo.com", "/simple_page3.html");
  NavigateAndWaitForCompletion(saved_url, shell());
  content::WebContents* contents = GetWebContents();
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());
  NavigateAndWaitForCompletion(saved2_url, shell());
  EXPECT_TRUE(contents->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());
  NavigateAndWaitForCompletion(foo_url, shell());
  EXPECT_FALSE(contents->GetPrimaryMainFrame()
                   ->GetSiteInstance()
                   ->RequiresDedicatedProcess());
}
#endif

IN_PROC_BROWSER_TEST_F(SiteIsolationBrowserTest, IsolatedSiteIsSavedOnlyOnce) {
  GURL saved_url =
      embedded_test_server()->GetURL("saved.com", "/simple_page.html");
  StartIsolatingSite(saved_url);
  StartIsolatingSite(saved_url);
  StartIsolatingSite(saved_url);
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("http://saved.com"));
}

// Failing on Android, see https://crbug.com/1254509.
#if defined(ANDROID)
#define MAYBE_ClearSiteDataHeaderDoesNotClearSavedIsolatedSites \
  ClearSiteDataHeaderDoesNotClearSavedIsolatedSites
#else
#define MAYBE_ClearSiteDataHeaderDoesNotClearSavedIsolatedSites \
  ClearSiteDataHeaderDoesNotClearSavedIsolatedSites
#endif
// Verify that serving a Clear-Site-Data header does not clear saved isolated
// sites. Saved isolated sites should only be cleared by user-initiated actions.
IN_PROC_BROWSER_TEST_F(
    SiteIsolationBrowserTest,
    MAYBE_ClearSiteDataHeaderDoesNotClearSavedIsolatedSites) {
  // Start an HTTPS server, as Clear-Site-Data is only available on HTTPS URLs.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));
  ASSERT_TRUE(https_server.Start());

  // Isolate saved.com and verify it's been saved to disk.
  GURL saved_url = https_server.GetURL("saved.com", "/clear_site_data.html");
  StartIsolatingSite(saved_url);
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("https://saved.com"));

  // Navigate to a URL that serves a Clear-Site-Data header for cache, cookies,
  // and DOM storage. This is the most that a Clear-Site-Data header could
  // clear, and this should not clear saved isolated sites.
  NavigateAndWaitForCompletion(saved_url, shell());
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("https://saved.com"));
}

IN_PROC_BROWSER_TEST_F(SiteIsolationBrowserTest,
                       ExplicitClearBrowsingDataClearsSavedIsolatedSites) {
  GURL saved_url =
      embedded_test_server()->GetURL("saved.com", "/simple_page.html");
  StartIsolatingSite(saved_url);
  EXPECT_THAT(GetSavedIsolatedSites(),
              UnorderedElementsAre("http://saved.com"));

  base::RunLoop run_loop;
  base::Time now = base::Time::Now();
  GetProfile()->ClearBrowsingData({BrowsingDataType::COOKIES_AND_SITE_DATA},
                                  now - base::Days(1), now,
                                  run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_THAT(GetSavedIsolatedSites(), IsEmpty());
}

}  // namespace weblayer
