// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/test/url_loader_interceptor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/error_page.h"
#include "weblayer/public/error_page_delegate.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/test_navigation_observer.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

#include "base/run_loop.h"

namespace weblayer {

namespace {

class TestErrorPageDelegate : public ErrorPageDelegate {
 public:
  void set_error_page_content(const std::string& value) { content_ = value; }
  // ErrorPageDelegate:
  bool OnBackToSafety() override { return false; }
  std::unique_ptr<ErrorPage> GetErrorPageContent(
      Navigation* navigation) override {
    if (!content_.has_value())
      return nullptr;
    auto error_page = std::make_unique<ErrorPage>();
    error_page->html = *content_;
    return error_page;
  }

 private:
  absl::optional<std::string> content_;
};

}  // namespace

using NavigationErrorNavigationThrottleBrowserTest = WebLayerBrowserTest;

// Verifies the delegate can inject an error page.
IN_PROC_BROWSER_TEST_F(NavigationErrorNavigationThrottleBrowserTest,
                       InjectErrorPage) {
  GURL url("http://doesntexist.com/foo");
  auto interceptor = content::URLLoaderInterceptor::SetupRequestFailForURL(
      url, net::ERR_NAME_NOT_RESOLVED);
  TestErrorPageDelegate delegate;
  delegate.set_error_page_content("<html><head><title>test error</title>");
  shell()->tab()->SetErrorPageDelegate(&delegate);
  NavigateAndWaitForFailure(url, shell());
  EXPECT_EQ(u"test error", GetTitle(shell()));
}

// Verifies the delegate can inject an empty page.
IN_PROC_BROWSER_TEST_F(NavigationErrorNavigationThrottleBrowserTest,
                       InjectEmptyErrorPage) {
  GURL url("http://doesntexist.com/foo");
  auto interceptor = content::URLLoaderInterceptor::SetupRequestFailForURL(
      url, net::ERR_NAME_NOT_RESOLVED);
  TestErrorPageDelegate delegate;
  delegate.set_error_page_content(std::string());
  shell()->tab()->SetErrorPageDelegate(&delegate);
  NavigateAndWaitForFailure(url, shell());
  base::Value body_text =
      ExecuteScript(shell()->tab(), "document.body.textContent", false);
  ASSERT_TRUE(body_text.is_string());
  EXPECT_TRUE(body_text.GetString().empty());
}

// Verifies a null return value results in a default error page.
// Network errors only have non-empty error pages on android.
#if BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(NavigationErrorNavigationThrottleBrowserTest,
                       DefaultErrorPage) {
  GURL url("http://doesntexist.com/foo");
  auto interceptor = content::URLLoaderInterceptor::SetupRequestFailForURL(
      url, net::ERR_NAME_NOT_RESOLVED);
  TestErrorPageDelegate delegate;
  shell()->tab()->SetErrorPageDelegate(&delegate);
  NavigateAndWaitForFailure(url, shell());
  base::Value body_text =
      ExecuteScript(shell()->tab(), "document.body.textContent", false);
  ASSERT_TRUE(body_text.is_string());
  EXPECT_FALSE(body_text.GetString().empty());
}
#endif

}  // namespace weblayer
