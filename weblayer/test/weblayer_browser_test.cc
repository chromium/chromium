// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/shell/common/shell_switches.h"

namespace weblayer {

WebLayerBrowserTest::WebLayerBrowserTest() {
  CreateTestServer(base::FilePath(FILE_PATH_LITERAL("weblayer/test/data")));
}

WebLayerBrowserTest::~WebLayerBrowserTest() = default;

void WebLayerBrowserTest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kNoInitialNavigation);
  SetUpCommandLine(command_line);
  content::BrowserTestBase::SetUp();
}

void WebLayerBrowserTest::PreRunTestOnMainThread() {
  ASSERT_EQ(Shell::windows().size(), 1u);
  shell_ = Shell::windows()[0];
}

void WebLayerBrowserTest::PostRunTestOnMainThread() {
  Shell::CloseAllWindows();
}

}  // namespace weblayer
