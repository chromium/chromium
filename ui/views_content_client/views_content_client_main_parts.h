// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_MAIN_PARTS_H_
#define UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_MAIN_PARTS_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/browser_main_parts.h"

namespace base {
class RunLoop;
}

namespace content {
class ShellBrowserContext;
struct MainFunctionParams;
}

namespace views {
class ViewsDelegate;
}

namespace ui {

class ViewsContentClient;

class ViewsContentClientMainParts : public content::BrowserMainParts {
 public:
  // Platform-specific create function.
  static std::unique_ptr<ViewsContentClientMainParts> Create(
      const content::MainFunctionParams& content_params,
      ViewsContentClient* views_content_client);

  // Invoked before the BrowserMainLoop constructor.
  static void PreCreateMainMessageLoop();

  ~ViewsContentClientMainParts() override;

  // content::BrowserMainParts:
  void PreMainMessageLoopRun() override;
  bool MainMessageLoopRun(int* result_code) override;
  void PostMainMessageLoopRun() override;

  content::ShellBrowserContext* browser_context() {
    return browser_context_.get();
  }

  ViewsContentClient* views_content_client() {
    return views_content_client_;
  }

 protected:
  ViewsContentClientMainParts(
      const content::MainFunctionParams& content_params,
      ViewsContentClient* views_content_client);

 private:
  std::unique_ptr<content::ShellBrowserContext> browser_context_;

  std::unique_ptr<views::ViewsDelegate> views_delegate_;

  ViewsContentClient* views_content_client_;

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ViewsContentClientMainParts);
};

}  // namespace ui

#endif  // UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_MAIN_PARTS_H_
