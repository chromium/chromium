// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

IN_PROC_BROWSER_TEST_F(WebLayerBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/simple_page.html");

  NavigateAndWaitForCompletion(url, shell());
}

}  // namespace weblayer
