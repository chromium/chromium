// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {
namespace {
const char kPrefetchPage[] = "/simple_prefetch.html";
const char kRedirectPrefetchPage[] = "/redirect_prefetch.html";
const char kRedirectPrefetchUrl[] = "/redirect";
const char kRedirectedPrefetchUrl[] = "/redirected";
const char kPrefetchTarget[] = "/prefetch_target.lnk";
}  // namespace

class PrefetchBrowserTest : public WebLayerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // The test makes requests to google.com which we want to redirect to the
    // test server.
    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &PrefetchBrowserTest::MonitorRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Set a dummy variation ID to send X-Client-Data header to Google hosts
    // in RedirectedPrefetch test.
    command_line->AppendSwitchASCII("force-variation-ids", "42");
    // Need to ignore cert errors to use a HTTPS server for the test domains.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  bool RunPrefetchExperiment(GURL url, const std::u16string expected_title) {
    content::TitleWatcher title_watcher(
        static_cast<TabImpl*>(shell()->tab())->web_contents(), expected_title);
    NavigateAndWaitForCompletion(url, shell());
    return expected_title == title_watcher.WaitAndGetTitle();
  }

 protected:
  bool prefetch_target_request_seen_ = false;
  base::Lock lock_;

  // |requests_| is accessed on the UI thread by the test body and on the IO
  // thread by the test server's request handler, so must be guarded by a lock
  // to avoid data races.
  std::vector<net::test_server::HttpRequest> requests_ GUARDED_BY(lock_);

 private:
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    if (request.relative_url == std::string(kPrefetchTarget)) {
      prefetch_target_request_seen_ = true;
    }
  }
};

IN_PROC_BROWSER_TEST_F(PrefetchBrowserTest, PrefetchWorks) {
  // Set real NetworkChangeNotifier singleton aside.
  std::unique_ptr<net::NetworkChangeNotifier::DisableForTest> disable_for_test(
      new net::NetworkChangeNotifier::DisableForTest);
  ASSERT_FALSE(prefetch_target_request_seen_);
  EXPECT_TRUE(RunPrefetchExperiment(
      embedded_test_server()->GetURL(kPrefetchPage), u"link onload"));
  EXPECT_TRUE(prefetch_target_request_seen_);
}

// https://crbug.com/922362: When the prefetched request is redirected, DCHECKs
// in PrefetchURLLoader::FollowRedirect() failed due to "X-Client-Data" in
// removed_headers. Verify that it no longer does.
IN_PROC_BROWSER_TEST_F(PrefetchBrowserTest, RedirectedPrefetch) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [this](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        base::AutoLock auto_lock(lock_);
        if (request.relative_url == std::string(kRedirectPrefetchPage)) {
          requests_.push_back(request);
          response->set_content_type("text/html");
          response->set_content(
              base::StringPrintf("<link rel=\"prefetch\" href=\"%s\" "
                                 "onload=\"document.title='done'\">",
                                 kRedirectPrefetchUrl));
          return response;
        } else if (request.relative_url == std::string(kRedirectPrefetchUrl)) {
          requests_.push_back(request);
          response->set_code(net::HTTP_MOVED_PERMANENTLY);
          response->AddCustomHeader(
              "Location", base::StringPrintf("https://example.com:%s%s",
                                             request.GetURL().port().c_str(),
                                             kRedirectedPrefetchUrl));
          return response;
        } else if (request.relative_url ==
                   std::string(kRedirectedPrefetchUrl)) {
          requests_.push_back(request);
          return response;
        }
        return nullptr;
      }));

  https_server.ServeFilesFromSourceDirectory(
      base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));
  {
    base::AutoLock auto_lock(lock_);
    requests_.clear();
  }
  ASSERT_TRUE(https_server.Start());

  GURL url = https_server.GetURL("www.google.com", kRedirectPrefetchPage);
  EXPECT_TRUE(RunPrefetchExperiment(url, u"done"));
  {
    base::AutoLock auto_lock(lock_);
    ASSERT_EQ(3U, requests_.size());
    EXPECT_EQ(base::StringPrintf("www.google.com:%u", https_server.port()),
              requests_[0].headers["Host"]);
    EXPECT_EQ(kRedirectPrefetchPage, requests_[0].relative_url);
  }
}

}  // namespace weblayer
