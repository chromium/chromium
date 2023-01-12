// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_H_
#define UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_H_

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
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
// #if BUILDFLAG(IS_WIN)
// int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
//   sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
//   content::InitializeSandboxInfo(&sandbox_info);
//   ui::ViewsContentClient params(instance, &sandbox_info);
// #else
// int main(int argc, const char** argv) {
//   ui::ViewsContentClient params(argc, argv);
// #endif
//
//   params.set_on_pre_main_message_loop_run_callback(
//       base::BindOnce(&InitMyApp));
//   return params.RunMain();
// }
class VIEWS_CONTENT_CLIENT_EXPORT ViewsContentClient {
 public:
  using OnPreMainMessageLoopRunCallback =
      base::OnceCallback<void(content::BrowserContext* browser_context,
                              gfx::NativeWindow window_context)>;

#if BUILDFLAG(IS_WIN)
  ViewsContentClient(HINSTANCE instance,
                     sandbox::SandboxInterfaceInfo* sandbox_info);
#else
  ViewsContentClient(int argc, const char** argv);
#endif

  ViewsContentClient(const ViewsContentClient&) = delete;
  ViewsContentClient& operator=(const ViewsContentClient&) = delete;

  ~ViewsContentClient();

  // Runs content::ContentMain() using the ExamplesMainDelegate.
  int RunMain();

  // The task to run at the end of BrowserMainParts::PreMainMessageLoopRun().
  // Ignored if this is not the main process.
  void set_on_pre_main_message_loop_run_callback(
      OnPreMainMessageLoopRunCallback callback) {
    on_pre_main_message_loop_run_callback_ = std::move(callback);
  }

  void set_on_resources_loaded_callback(base::OnceClosure callback) {
    on_resources_loaded_callback_ = std::move(callback);
  }

  // Calls the OnPreMainMessageLoopRun callback. |browser_context| is the
  // current browser context. |window_context| is a candidate root window that
  // may be null.
  void OnPreMainMessageLoopRun(content::BrowserContext* browser_context,
                               gfx::NativeWindow window_context);

  // Calls a callback to signal resources have been loaded.
  void OnResourcesLoaded();

  // Called by ViewsContentClientMainParts to supply the quit-closure to use
  // to exit RunMain().
  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }
  base::OnceClosure& quit_closure() { return quit_closure_; }

 private:
#if BUILDFLAG(IS_WIN)
  HINSTANCE instance_;
  raw_ptr<sandbox::SandboxInterfaceInfo> sandbox_info_;
#else
  int argc_;
  raw_ptr<const char*> argv_;
#endif
  OnPreMainMessageLoopRunCallback on_pre_main_message_loop_run_callback_;
  base::OnceClosure on_resources_loaded_callback_;
  base::OnceClosure quit_closure_;
};

}  // namespace ui

#endif  // UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_H_
