// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/host_content_settings_map_factory.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class ClientHintsBrowserTest : public WebLayerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    WebLayerBrowserTest::SetUpOnMainThread();
    BrowserProcess::GetInstance()
        ->GetNetworkQualityTracker()
        ->ReportRTTsAndThroughputForTesting(base::Milliseconds(500), 100);

    EXPECT_TRUE(embedded_test_server()->Start());
  }

  void SetAcceptClientHints() {
    NavigateAndWaitForCompletion(
        embedded_test_server()->GetURL(
            "/set-header?Accept-CH: device-memory,rtt"),
        shell());
  }

  void CheckNavigationHeaders() {
    NavigateAndWaitForCompletion(
        embedded_test_server()->GetURL("/echoheader?device-memory"), shell());

    double device_memory = 0.0;
    ASSERT_TRUE(base::StringToDouble(GetBody(), &device_memory));
    EXPECT_GT(device_memory, 0.0);

    NavigateAndWaitForCompletion(
        embedded_test_server()->GetURL("/echoheader?rtt"), shell());
    int rtt = 0;
    ASSERT_TRUE(base::StringToInt(GetBody(), &rtt));
    EXPECT_GT(rtt, 0);
  }

  void CheckSubresourceHeaders() {
    double device_memory = 0.0;
    ASSERT_TRUE(base::StringToDouble(GetSubresourceHeader("device-memory"),
                                     &device_memory));
    EXPECT_GT(device_memory, 0.0);

    int rtt = 0;
    ASSERT_TRUE(base::StringToInt(GetSubresourceHeader("rtt"), &rtt));
    EXPECT_GT(rtt, 0);
  }

  void KillRenderer() {
    content::RenderProcessHost* child_process =
        static_cast<TabImpl*>(shell()->tab())
            ->web_contents()
            ->GetPrimaryMainFrame()
            ->GetProcess();
    content::RenderProcessHostWatcher crash_observer(
        child_process,
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    child_process->Shutdown(0);
    crash_observer.Wait();
  }

  std::string GetSubresourceHeader(const std::string& header) {
    constexpr char kScript[] = R"(
      new Promise(function (resolve, reject) {
        const xhr = new XMLHttpRequest();
        xhr.open("GET", "/echoheader?" + $1);
        xhr.onload = () => {
          resolve(xhr.response);
        };
        xhr.send();
      })
    )";
    content::WebContents* web_contents =
        static_cast<TabImpl*>(shell()->tab())->web_contents();
    return content::EvalJs(web_contents,
                           content::JsReplace(kScript, "device-memory"))
        .ExtractString();
  }

  std::string GetBody() {
    return ExecuteScript(shell(), "document.body.innerText", true).GetString();
  }
};

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, Navigation) {
  SetAcceptClientHints();
  CheckNavigationHeaders();
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, Subresource) {
  SetAcceptClientHints();
  CheckSubresourceHeaders();
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest, SubresourceInNewRenderer) {
  SetAcceptClientHints();
  KillRenderer();
  NavigateAndWaitForCompletion(embedded_test_server()->GetURL("/echo"),
                               shell());
  CheckSubresourceHeaders();
}

IN_PROC_BROWSER_TEST_F(ClientHintsBrowserTest,
                       SubresourceAfterContentSettingUpdate) {
  // Set accept client hints on the original server.
  SetAcceptClientHints();

  // Start a new server which will not accept client hints.
  net::test_server::EmbeddedTestServer other_server;
  net::test_server::RegisterDefaultHandlers(&other_server);
  ASSERT_TRUE(other_server.Start());
  NavigateAndWaitForCompletion(other_server.GetURL("/echo"), shell());
  EXPECT_EQ(GetSubresourceHeader("device-memory"), "None");

  // Copy client hints over to the other server.
  auto* settings_map = HostContentSettingsMapFactory::GetForBrowserContext(
      static_cast<TabImpl*>(shell()->tab())
          ->web_contents()
          ->GetBrowserContext());
  base::Value setting = settings_map->GetWebsiteSetting(
      embedded_test_server()->base_url(), GURL(),
      ContentSettingsType::CLIENT_HINTS, nullptr);
  ASSERT_FALSE(setting.is_none());
  settings_map->SetWebsiteSettingDefaultScope(other_server.base_url(), GURL(),
                                              ContentSettingsType::CLIENT_HINTS,
                                              setting.Clone());

  // Settings take affect after navigation only, so the header shouldn't be
  // there yet.
  EXPECT_EQ(GetSubresourceHeader("device-memory"), "None");

  // After re-navigating, should have hints.
  NavigateAndWaitForCompletion(other_server.GetURL("/echo"), shell());
  CheckSubresourceHeaders();
}

}  // namespace weblayer
