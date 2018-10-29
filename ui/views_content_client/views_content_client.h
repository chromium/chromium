// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_H_
#define UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views_content_client/views_content_client_export.h"

namespace content {
class BrowserContext;
}

namespace sandbox {
struct SandboxInterfaceInfo;
}

namespace ui {

// Creates a multiprocess views runtime for running an example application.
//
// Sample usage:
//
// void InitMyApp(content::BrowserContext* browser_context,
//                gfx::NativeWindow window_context) {
//   // Create desired windows and views here. Runs on the UI thread.
// }
//
// #if defined(OS_WIN)
// int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
//   sandbox::SandboxInterfaceInfo sandbox_info = {0};
//   content::InitializeSandboxInfo(&sandbox_info);
//   ui::ViewsContentClient params(instance, &sandbox_info);
// #else
// int main(int argc, const char** argv) {
//   ui::ViewsContentClient params(argc, argv);
// #endif
//
//   params.set_task(base::Bind(&InitMyApp));
//   return params.RunMain();
// }
class VIEWS_CONTENT_CLIENT_EXPORT ViewsContentClient {
 public:
  typedef base::Callback<
      void(content::BrowserContext* browser_context,
           gfx::NativeWindow window_context)> Task;

#if defined(OS_WIN)
  ViewsContentClient(HINSTANCE instance,
                     sandbox::SandboxInterfaceInfo* sandbox_info);
#else
  ViewsContentClient(int argc, const char** argv);
#endif

  ~ViewsContentClient();

  // Runs content::ContentMain() using the ExamplesMainDelegate.
  int RunMain();

  // The task to run at the end of BrowserMainParts::PreMainMessageLoopRun().
  // Ignored if this is not the main process.
  void set_task(const Task& task) { task_ = task; }
  const Task& task() const { return task_; }

  // Called by ViewsContentClientMainParts to supply the quit-closure to use
  // to exit RunMain().
  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }
  base::OnceClosure& quit_closure() { return quit_closure_; }

 private:
#if defined(OS_WIN)
  HINSTANCE instance_;
  sandbox::SandboxInterfaceInfo* sandbox_info_;
#else
  int argc_;
  const char** argv_;
#endif
  Task task_;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ViewsContentClient);
};

}  // namespace ui

#endif  // UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_H_
