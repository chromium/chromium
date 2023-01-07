// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/test/weblayer_browser_test.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "components/embedder_support/switches.h"
#include "content/public/browser/browser_context.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/common/features.h"
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

  // Disable auto reload since most browser tests do not expect error pages to
  // reload automatically. Tests that want auto reload can explicitly append
  // embedder_support::kEnableAutoReload, which will override the disable here.
  command_line->AppendSwitch(embedder_support::kDisableAutoReload);

  if (start_in_incognito_mode_)
    command_line->AppendSwitch(switches::kStartInIncognito);

  SetUpCommandLine(command_line);
  content::BrowserTestBase::SetUp();
}

void WebLayerBrowserTest::PreRunTestOnMainThread() {
  ASSERT_EQ(Shell::windows().size(), 1u);
  shell_ = Shell::windows()[0];

  // Don't fill machine's download directory from tests; instead place downloads
  // in the temporary user-data-dir for this test.
  auto* tab_impl = static_cast<TabImpl*>(shell_->tab());
  auto* browser_context = tab_impl->web_contents()->GetBrowserContext();
  auto* browser_context_impl =
      static_cast<BrowserContextImpl*>(browser_context);
  browser_context_impl->profile_impl()->SetDownloadDirectory(
      browser_context->GetPath());
  // Accessing a browser context may involve storage partition initialization.
  // Wait for the initialization to be completed.
  base::RunLoop().RunUntilIdle();
}

void WebLayerBrowserTest::PostRunTestOnMainThread() {
  Shell::CloseAllWindows();
}

void WebLayerBrowserTest::SetShellStartsInIncognitoMode() {
  DCHECK(!set_up_called());
  start_in_incognito_mode_ = true;
}

ProfileImpl* WebLayerBrowserTest::GetProfile() {
  return static_cast<TabImpl*>(shell_->tab())->profile();
}

content::BrowserContext* WebLayerBrowserTest::GetBrowserContext() {
  return GetProfile()->GetBrowserContext();
}

}  // namespace weblayer
