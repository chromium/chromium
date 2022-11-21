// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webapps/webapk_install_scheduler.h"

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"

// Keep these tests in sync with tests for building the WebAPK-proto in
// chrome/browser/android/webapk/webapk_installer_unittest.cc.

namespace {

const base::FilePath::CharType kTestDataDir[] =
    FILE_PATH_LITERAL("components/test/data/webapps");

// Start URL for the WebAPK
const char* kStartUrl = "/index.html";

// The URLs of best icons from Web Manifest. We use a random file in the test
// data directory. Since WebApkInstallScheduler does not try to decode the file
// as an image it is OK that the file is not an image.
const char* kBestPrimaryIconUrl = "/simple.html";
const char* kBestSplashIconUrl = "/nostore.html";
const char* kBestShortcutIconUrl = "/title1.html";

// Icon which has Cross-Origin-Resource-Policy: same-origin set.
const char* kBestPrimaryIconCorpUrl = "/cors_same_origin.png";

}  // namespace

namespace weblayer {

class TestWebApkInstallScheduler : public WebApkInstallScheduler {
 public:
  TestWebApkInstallScheduler(const webapps::ShortcutInfo& shortcut_info,
                             const SkBitmap& primary_icon,
                             bool is_primary_icon_maskable,
                             WebApkInstallFinishedCallback callback)
      : WebApkInstallScheduler(shortcut_info,
                               primary_icon,
                               is_primary_icon_maskable,
                               std::move(callback)) {}

  TestWebApkInstallScheduler(const TestWebApkInstallScheduler&) = delete;
  TestWebApkInstallScheduler& operator=(const TestWebApkInstallScheduler&) =
      delete;

  void ScheduleWithChrome(
      std::unique_ptr<std::string> serialized_proto) override {
    PostTaskToRunSuccessCallback();
  }

  // Function used for testing FetchProtoAndScheduleInstall. |callback| can
  // be set to forward the result-value in the OnResult-callback to a test for
  // verification.
  void FetchProtoAndScheduleInstallForTesting(
      content::WebContents* web_contents,
      WebApkInstallScheduler::FinishCallback callback) {
    callback_ = std::move(callback);

    WebApkInstallScheduler::FetchProtoAndScheduleInstallForTesting(
        web_contents);
  }

  void OnResult(webapps::WebApkInstallResult result) override {
    // Pass the |result| to the callback for verification.
    std::move(callback_).Run(result);

    WebApkInstallScheduler::OnResult(result);
  }

  void PostTaskToRunSuccessCallback() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&TestWebApkInstallScheduler::OnResult,
                                  base::Unretained(this),
                                  webapps::WebApkInstallResult::SUCCESS));
  }

 private:
  WebApkInstallScheduler::FinishCallback callback_;
};

// Wrapper class for running WebApkInstallScheduler#FetchProtoAndScheduleInstall
// that makes the WebApkInstallResult that is received in the OnResult-callback
// accessible for testing.
class WebApkInstallSchedulerRunner {
 public:
  WebApkInstallSchedulerRunner() {}

  WebApkInstallSchedulerRunner(const WebApkInstallSchedulerRunner&) = delete;
  WebApkInstallSchedulerRunner& operator=(const WebApkInstallSchedulerRunner&) =
      delete;

  ~WebApkInstallSchedulerRunner() {}

  void RunFetchProtoAndScheduleInstall(
      std::unique_ptr<TestWebApkInstallScheduler> fetcher,
      content::WebContents* web_contents) {
    base::RunLoop run_loop;
    on_completed_callback_ = run_loop.QuitClosure();

    // WebApkInstallScheduler owns itself.
    fetcher.release()->FetchProtoAndScheduleInstallForTesting(
        web_contents, base::BindOnce(&WebApkInstallSchedulerRunner::OnCompleted,
                                     base::Unretained(this)));

    run_loop.Run();
  }

  webapps::WebApkInstallResult result() { return result_; }

 private:
  void OnCompleted(webapps::WebApkInstallResult result) {
    result_ = result;
    std::move(on_completed_callback_).Run();
  }

  // Called after the installation process has succeeded or failed.
  base::OnceClosure on_completed_callback_;

  // The result of the installation process.
  webapps::WebApkInstallResult result_;
};

class WebApkInstallSchedulerTest : public WebLayerBrowserTest {
 public:
  WebApkInstallSchedulerTest() = default;
  ~WebApkInstallSchedulerTest() override = default;

  WebApkInstallSchedulerTest(const WebApkInstallSchedulerTest&) = delete;
  WebApkInstallSchedulerTest& operator=(const WebApkInstallSchedulerTest&) =
      delete;

  void SetUpOnMainThread() override {
    WebLayerBrowserTest::SetUpOnMainThread();

    web_contents_ = static_cast<TabImpl*>(shell()->tab())->web_contents();

    test_server_.AddDefaultHandlers(base::FilePath(kTestDataDir));
    ASSERT_TRUE(test_server_.Start());
  }

  content::WebContents* web_contents() { return web_contents_; }

  net::test_server::EmbeddedTestServer* test_server() { return &test_server_; }

  std::unique_ptr<TestWebApkInstallScheduler> DefaultWebApkInstallScheduler(
      webapps::ShortcutInfo info) {
    std::unique_ptr<TestWebApkInstallScheduler> scheduler_bridge(
        new TestWebApkInstallScheduler(
            info, SkBitmap(), false,
            base::BindOnce(&WebApkInstallSchedulerTest::OnInstallFinished,
                           base::Unretained(this))));
    return scheduler_bridge;
  }

  webapps::ShortcutInfo DefaultShortcutInfo() {
    webapps::ShortcutInfo info(test_server_.GetURL(kStartUrl));
    info.best_primary_icon_url = test_server_.GetURL(kBestPrimaryIconUrl);
    info.splash_image_url = test_server_.GetURL(kBestSplashIconUrl);
    info.best_shortcut_icon_urls.push_back(
        test_server_.GetURL(kBestShortcutIconUrl));
    return info;
  }

 private:
  raw_ptr<content::WebContents> web_contents_;

  net::EmbeddedTestServer test_server_;

  void OnInstallFinished(GURL manifest_url, GURL manifest_id) {}
};

// Test building the WebAPK-proto is succeeding.
IN_PROC_BROWSER_TEST_F(WebApkInstallSchedulerTest, Success) {
  WebApkInstallSchedulerRunner runner;
  runner.RunFetchProtoAndScheduleInstall(
      DefaultWebApkInstallScheduler(DefaultShortcutInfo()), web_contents());
  EXPECT_EQ(webapps::WebApkInstallResult::SUCCESS, runner.result());
}

// Test that building the WebAPK-proto succeeds when the primary icon is guarded
// by a Cross-Origin-Resource-Policy: same-origin header and the icon is
// same-origin with the start URL.
IN_PROC_BROWSER_TEST_F(WebApkInstallSchedulerTest,
                       CrossOriginResourcePolicySameOriginIconSuccess) {
  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();
  shortcut_info.best_primary_icon_url =
      test_server()->GetURL(kBestPrimaryIconCorpUrl);

  WebApkInstallSchedulerRunner runner;
  runner.RunFetchProtoAndScheduleInstall(
      DefaultWebApkInstallScheduler(shortcut_info), web_contents());
  EXPECT_EQ(webapps::WebApkInstallResult::SUCCESS, runner.result());
}

// Test that building the WebAPK-proto fails if fetching the bitmap at the best
// primary icon URL returns no content. In a perfect world the fetch would
// always succeed because the fetch for the same icon succeeded recently.
IN_PROC_BROWSER_TEST_F(WebApkInstallSchedulerTest,
                       BestPrimaryIconUrlDownloadTimesOut) {
  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();
  shortcut_info.best_primary_icon_url = test_server()->GetURL("/nocontent");

  WebApkInstallSchedulerRunner runner;
  runner.RunFetchProtoAndScheduleInstall(
      DefaultWebApkInstallScheduler(shortcut_info), web_contents());
  EXPECT_EQ(webapps::WebApkInstallResult::ICON_HASHER_ERROR, runner.result());
}

// Test that building the WebAPK-proto fails if fetching the bitmap at the best
// splash icon URL returns no content. In a perfect world the fetch would always
// succeed because the fetch for the same icon succeeded recently.
IN_PROC_BROWSER_TEST_F(WebApkInstallSchedulerTest,
                       BestSplashIconUrlDownloadTimesOut) {
  webapps::ShortcutInfo shortcut_info = DefaultShortcutInfo();
  shortcut_info.best_primary_icon_url = test_server()->GetURL("/nocontent");

  WebApkInstallSchedulerRunner runner;
  runner.RunFetchProtoAndScheduleInstall(
      DefaultWebApkInstallScheduler(shortcut_info), web_contents());
  EXPECT_EQ(webapps::WebApkInstallResult::ICON_HASHER_ERROR, runner.result());
}

}  // namespace weblayer
