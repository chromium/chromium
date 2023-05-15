// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/allocator/partition_allocator/tagging.h"
#include "base/cpu.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

IN_PROC_BROWSER_TEST_F(WebLayerBrowserTest, SynchronousMemoryTagging) {
  // weblayer_browsertests should start up in synchronous MTE mode
  base::CPU cpu;
  if (cpu.has_mte()) {
    ASSERT_EQ(partition_alloc::internal::GetMemoryTaggingModeForCurrentThread(),
              partition_alloc::TagViolationReportingMode::kSynchronous);

  } else {
    GTEST_SKIP();
  }
}

IN_PROC_BROWSER_TEST_F(WebLayerBrowserTest, Basic) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/simple_page.html");

  NavigateAndWaitForCompletion(url, shell());
}

}  // namespace weblayer
