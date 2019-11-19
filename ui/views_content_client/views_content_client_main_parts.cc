// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views_content_client/views_content_client_main_parts.h"

#include <utility>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/public/browser/context_factory.h"
#include "content/shell/browser/shell_browser_context.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/views/test/desktop_test_views_delegate.h"
#include "ui/views_content_client/views_content_client.h"

namespace ui {

ViewsContentClientMainParts::ViewsContentClientMainParts(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client)
    : views_content_client_(views_content_client) {
}

ViewsContentClientMainParts::~ViewsContentClientMainParts() {
}

#if !defined(OS_MACOSX)
void ViewsContentClientMainParts::PreCreateMainMessageLoop() {}
#endif

void ViewsContentClientMainParts::PreMainMessageLoopRun() {
  ui::MaterialDesignController::Initialize();
  ui::InitializeInputMethodForTesting();
  browser_context_ = std::make_unique<content::ShellBrowserContext>(false);

  std::unique_ptr<views::TestViewsDelegate> test_views_delegate(
      new views::DesktopTestViewsDelegate);
  test_views_delegate->set_context_factory(content::GetContextFactory());
  test_views_delegate->set_context_factory_private(
      content::GetContextFactoryPrivate());
  views_delegate_ = std::move(test_views_delegate);
  run_loop_ = std::make_unique<base::RunLoop>();
  views_content_client()->set_quit_closure(run_loop_->QuitClosure());
}

void ViewsContentClientMainParts::PostMainMessageLoopRun() {
  browser_context_.reset();
  views_delegate_.reset();
}

bool ViewsContentClientMainParts::MainMessageLoopRun(int* result_code) {
  run_loop_->Run();
  return true;
}

}  // namespace ui
