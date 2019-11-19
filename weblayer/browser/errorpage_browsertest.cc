// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

#if defined(OS_ANDROID)
#include "android_webview/grit/aw_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace weblayer {

using ErrorPageBrowserTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(ErrorPageBrowserTest, NameNotResolved) {
  GURL error_page_url =
      net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_NAME_NOT_RESOLVED);

  NavigateAndWaitForFailure(error_page_url, shell());

  // Currently, interstitials for error pages are displayed only on Android.
#if defined(OS_ANDROID)
  base::string16 expected_title =
      l10n_util::GetStringUTF16(IDS_AW_WEBPAGE_NOT_AVAILABLE);
  EXPECT_EQ(expected_title, GetTitle(shell()));
#endif
}

// Verifies that navigating to a URL that returns a 404 with an empty body
// results in the navigation failing.
IN_PROC_BROWSER_TEST_F(ErrorPageBrowserTest, 404WithEmptyBody) {
  EXPECT_TRUE(embedded_test_server()->Start());

  GURL error_page_url = embedded_test_server()->GetURL("/empty404.html");

  NavigateAndWaitForFailure(error_page_url, shell());
}

}  // namespace weblayer
