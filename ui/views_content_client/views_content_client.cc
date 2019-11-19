// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views_content_client/views_content_client.h"

#include <utility>

#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "ui/views_content_client/views_content_main_delegate.h"

namespace ui {

#if defined(OS_WIN)
ViewsContentClient::ViewsContentClient(
    HINSTANCE instance, sandbox::SandboxInterfaceInfo* sandbox_info)
    : instance_(instance), sandbox_info_(sandbox_info) {
}
#else
ViewsContentClient::ViewsContentClient(int argc, const char** argv)
    : argc_(argc), argv_(argv) {
}
#endif

ViewsContentClient::~ViewsContentClient() {
}

int ViewsContentClient::RunMain() {
  ViewsContentMainDelegate delegate(this);
  content::ContentMainParams params(&delegate);

#if defined(OS_WIN)
  params.instance = instance_;
  params.sandbox_info = sandbox_info_;
#else
  params.argc = argc_;
  params.argv = argv_;
#endif

  return content::ContentMain(params);
}

void ViewsContentClient::OnPreMainMessageLoopRun(
    content::BrowserContext* browser_context,
    gfx::NativeWindow window_context) {
  std::move(on_pre_main_message_loop_run_callback_)
      .Run(browser_context, window_context);
}

}  // namespace ui
