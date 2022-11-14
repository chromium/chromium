// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/slow_download_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/download_manager_delegate_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/download.h"
#include "weblayer/public/download_delegate.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/test_navigation_observer.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

namespace {

class DownloadBrowserTest : public WebLayerBrowserTest,
                            public DownloadDelegate {
 public:
  DownloadBrowserTest() = default;
  ~DownloadBrowserTest() override = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &content::SlowDownloadHttpResponse::HandleSlowDownloadRequest));
    ASSERT_TRUE(embedded_test_server()->Start());

    allow_run_loop_ = std::make_unique<base::RunLoop>();
    started_run_loop_ = std::make_unique<base::RunLoop>();
    intercept_run_loop_ = std::make_unique<base::RunLoop>();
    completed_run_loop_ = std::make_unique<base::RunLoop>();
    failed_run_loop_ = std::make_unique<base::RunLoop>();

    Tab* tab = shell()->tab();
    TabImpl* tab_impl = static_cast<TabImpl*>(tab);

    tab_impl->profile()->SetDownloadDelegate(this);

    auto* browser_context = tab_impl->web_contents()->GetBrowserContext();
    auto* download_manager_delegate =
        browser_context->GetDownloadManager()->GetDelegate();
    static_cast<DownloadManagerDelegateImpl*>(download_manager_delegate)
        ->set_download_dropped_closure_for_testing(base::BindRepeating(
            &DownloadBrowserTest::DownloadDropped, base::Unretained(this)));
  }

  void WaitForAllow() { allow_run_loop_->Run(); }
  void WaitForIntercept() { intercept_run_loop_->Run(); }
  void WaitForStarted() { started_run_loop_->Run(); }
  void WaitForCompleted() { completed_run_loop_->Run(); }
  void WaitForFailed() { failed_run_loop_->Run(); }

  void set_intercept() { intercept_ = true; }
  void set_disallow() { allow_ = false; }
  void set_started_callback(
      base::OnceCallback<void(Download* download)> callback) {
    started_callback_ = std::move(callback);
  }
  void set_failed_callback(
      base::OnceCallback<void(Download* download)> callback) {
    failed_callback_ = std::move(callback);
  }
  bool started() { return started_; }
  base::FilePath download_location() { return download_location_; }
  int64_t total_bytes() { return total_bytes_; }
  DownloadError download_state() { return download_state_; }
  std::string mime_type() { return mime_type_; }
  int completed_count() { return completed_count_; }
  int failed_count() { return failed_count_; }
  int download_dropped_count() { return download_dropped_count_; }

 private:
  // DownloadDelegate implementation:
  void AllowDownload(Tab* tab,
                     const GURL& url,
                     const std::string& request_method,
                     absl::optional<url::Origin> request_initiator,
                     AllowDownloadCallback callback) override {
    std::move(callback).Run(allow_);
    allow_run_loop_->Quit();
  }

  bool InterceptDownload(const GURL& url,
                         const std::string& user_agent,
                         const std::string& content_disposition,
                         const std::string& mime_type,
                         int64_t content_length) override {
    intercept_run_loop_->Quit();
    return intercept_;
  }

  void DownloadStarted(Download* download) override {
    started_ = true;
    started_run_loop_->Quit();

    CHECK_EQ(download->GetState(), DownloadState::kInProgress);

    if (started_callback_)
      std::move(started_callback_).Run(download);
  }

  void DownloadCompleted(Download* download) override {
    completed_count_++;
    download_location_ = download->GetLocation();
    total_bytes_ = download->GetTotalBytes();
    download_state_ = download->GetError();
    mime_type_ = download->GetMimeType();
    CHECK_EQ(download->GetReceivedBytes(), total_bytes_);
    CHECK_EQ(download->GetState(), DownloadState::kComplete);
    completed_run_loop_->Quit();
  }

  void DownloadFailed(Download* download) override {
    failed_count_++;
    download_state_ = download->GetError();
    failed_run_loop_->Quit();

    if (failed_callback_)
      std::move(failed_callback_).Run(download);
  }

  void DownloadDropped() { download_dropped_count_++; }

  bool intercept_ = false;
  bool allow_ = true;
  bool started_ = false;
  base::OnceCallback<void(Download* download)> started_callback_;
  base::OnceCallback<void(Download* download)> failed_callback_;
  base::FilePath download_location_;
  int64_t total_bytes_ = 0;
  DownloadError download_state_ = DownloadError::kNoError;
  std::string mime_type_;
  int completed_count_ = 0;
  int failed_count_ = 0;
  int download_dropped_count_ = 0;
  std::unique_ptr<base::RunLoop> allow_run_loop_;
  std::unique_ptr<base::RunLoop> intercept_run_loop_;
  std::unique_ptr<base::RunLoop> started_run_loop_;
  std::unique_ptr<base::RunLoop> completed_run_loop_;
  std::unique_ptr<base::RunLoop> failed_run_loop_;
};

}  // namespace

// Ensures that if the delegate disallows the downloads then WebLayer
// doesn't download it.
IN_PROC_BROWSER_TEST_F(DownloadBrowserTest, DisallowNoDownload) {
  set_disallow();

  GURL url(embedded_test_server()->GetURL("/content-disposition.html"));

  // Downloads always count as failed navigations.
  TestNavigationObserver observer(
      url, TestNavigationObserver::NavigationEvent::kFailure, shell());
  shell()->tab()->GetNavigationController()->Navigate(url);
  observer.Wait();

  WaitForAllow();

  EXPECT_FALSE(started());
  EXPECT_EQ(completed_count(), 0);
  EXPECT_EQ(failed_count(), 0);
  EXPECT_EQ(download_dropped_count(), 1);
}

// Ensures that if the delegate chooses to intercept downloads then WebLayer
// doesn't download it.
IN_PROC_BROWSER_TEST_F(DownloadBrowserTest, InterceptNoDownload) {
  set_intercept();

  GURL url(embedded_test_server()->GetURL("/content-disposition.html"));

  shell()->tab()->GetNavigationController()->Navigate(url);

  WaitForIntercept();

  EXPECT_FALSE(started());
  EXPECT_EQ(completed_count(), 0);
  EXPECT_EQ(failed_count(), 0);
  EXPECT_EQ(download_dropped_count(), 1);
}

IN_PROC_BROWSER_TEST_F(DownloadBrowserTest, Basic) {
  GURL url(embedded_test_server()->GetURL("/content-disposition.html"));

  shell()->tab()->GetNavigationController()->Navigate(url);

  WaitForCompleted();

  EXPECT_TRUE(started());
  EXPECT_EQ(completed_count(), 1);
  EXPECT_EQ(failed_count(), 0);
  EXPECT_EQ(download_dropped_count(), 0);
  EXPECT_EQ(download_state(), DownloadError::kNoError);
  EXPECT_EQ(mime_type(), "text/html");

  // Check that the size on disk matches what's expected.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    int64_t downloaded_file_size, original_file_size;
    EXPECT_TRUE(base::GetFileSize(download_location(), &downloaded_file_size));
    base::FilePath test_data_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    EXPECT_TRUE(base::GetFileSize(
        test_data_dir.Append(base::FilePath(
            FILE_PATH_LITERAL("weblayer/test/data/content-disposition.html"))),
        &original_file_size));
    EXPECT_EQ(downloaded_file_size, total_bytes());
  }

  // Ensure browser tests don't write to the default machine download directory
  // to avoid filing it up.
  EXPECT_NE(BrowserContextImpl::GetDefaultDownloadDirectory(),
            download_location().DirName());
}

// Test consistently failing on android: crbug.com/1273105
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_OverrideDownloadDirectory DISABLED_OverrideDownloadDirectory
#else
#define MAYBE_OverrideDownloadDirectory OverrideDownloadDirectory
#endif
IN_PROC_BROWSER_TEST_F(DownloadBrowserTest, MAYBE_OverrideDownloadDirectory) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir download_dir;
  ASSERT_TRUE(download_dir.CreateUniqueTempDir());

  TabImpl* tab_impl = static_cast<TabImpl*>(shell()->tab());
  auto* browser_context = tab_impl->web_contents()->GetBrowserContext();
  auto* browser_context_impl =
      static_cast<BrowserContextImpl*>(browser_context);
  browser_context_impl->profile_impl()->SetDownloadDirectory(
      download_dir.GetPath());

  GURL url(embedded_test_server()->GetURL("/content-disposition.html"));

  shell()->tab()->GetNavigationController()->Navigate(url);

  WaitForCompleted();

  EXPECT_EQ(completed_count(), 1);
  EXPECT_EQ(failed_count(), 0);
  EXPECT_EQ(download_dir.GetPath(), download_location().DirName());
}

IN_PROC_BROWSER_TEST_F(DownloadBrowserTest, Cancel) {
  set_started_callback(base::BindLambdaForTesting([&](Download* download) {
    download->Cancel();

    // Also allow the download to complete.
    GURL url = embedded_test_server()->GetURL(
        content::SlowDownloadHttpResponse::kFinishSlowResponseUrl);
    shell()->tab()->GetNavigationController()->Navigate(url);
  }));

  set_failed_callback(base::BindLambdaForTesting([](Download* download) {
    CHECK_EQ(download->GetState(), DownloadState::kCancelled);
  }));

  // Create a request that doesn't complete right away to avoid flakiness.
  GURL url(embedded_test_server()->GetURL(
      content::SlowDownloadHttpResponse::kKnownSizeUrl));

  shell()->tab()->GetNavigationController()->Navigate(url);

  WaitForFailed();
  EXPECT_EQ(completed_count(), 0);
  EXPECT_EQ(failed_count(), 1);
  EXPECT_EQ(download_dropped_count(), 0);
  EXPECT_EQ(download_state(), DownloadError::kCancelled);
}

// TODO(crbug.com/1314060): Flaky on Windows and Linux.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_PauseResume DISABLED_PauseResume
#else
#define MAYBE_PauseResume PauseResume
#endif
IN_PROC_BROWSER_TEST_F(DownloadBrowserTest, MAYBE_PauseResume) {
  // Add an initial navigation to avoid the tab being deleted if the first
  // navigation is a download, since we use the tab for convenience in the
  // lambda.
  OneShotNavigationObserver observer(shell());
  shell()->tab()->GetNavigationController()->Navigate(GURL("about:blank"));
  observer.WaitForNavigation();

  set_started_callback(base::BindLambdaForTesting([&](Download* download) {
    download->Pause();
    GURL url = embedded_test_server()->GetURL(
        content::SlowDownloadHttpResponse::kFinishSlowResponseUrl);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](Download* download, Shell* shell, const GURL& url) {
                         CHECK_EQ(download->GetState(), DownloadState::kPaused);
                         download->Resume();

                         // Also allow the download to complete.
                         shell->tab()->GetNavigationController()->Navigate(url);
                       },
                       download, shell(), url));
  }));

  // Create a request that doesn't complete right away to avoid flakiness.
  GURL url(embedded_test_server()->GetURL(
      content::SlowDownloadHttpResponse::kKnownSizeUrl));
  shell()->tab()->GetNavigationController()->Navigate(url);

  WaitForCompleted();
  EXPECT_EQ(completed_count(), 1);
  EXPECT_EQ(failed_count(), 0);
  EXPECT_EQ(download_dropped_count(), 0);
}

IN_PROC_BROWSER_TEST_F(DownloadBrowserTest, NetworkError) {
  set_failed_callback(base::BindLambdaForTesting([](Download* download) {
    CHECK_EQ(download->GetState(), DownloadState::kFailed);
  }));

  // Create a request that doesn't complete right away.
  GURL url(embedded_test_server()->GetURL(
      content::SlowDownloadHttpResponse::kKnownSizeUrl));

  shell()->tab()->GetNavigationController()->Navigate(url);

  WaitForStarted();
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  WaitForFailed();
  EXPECT_EQ(completed_count(), 0);
  EXPECT_EQ(failed_count(), 1);
  EXPECT_EQ(download_dropped_count(), 0);
  EXPECT_EQ(download_state(), DownloadError::kConnectivityError);
}

IN_PROC_BROWSER_TEST_F(DownloadBrowserTest, PendingOnExist) {
  // Create a request that doesn't complete right away.
  GURL url(embedded_test_server()->GetURL(
      content::SlowDownloadHttpResponse::kKnownSizeUrl));

  shell()->tab()->GetNavigationController()->Navigate(url);

  WaitForStarted();

  // If this test crashes later then there'd be a regression.
}

}  // namespace weblayer
