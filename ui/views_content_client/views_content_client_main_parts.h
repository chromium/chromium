// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_MAIN_PARTS_H_
#define UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_MAIN_PARTS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/browser_main_parts.h"

#if BUILDFLAG(IS_APPLE)
#include "ui/display/screen.h"
#endif

namespace base {
class RunLoop;
}

namespace content {
class ShellBrowserContext;
}

namespace views {
class TestViewsDelegate;
}

namespace ui {

class ViewsContentClient;

class ViewsContentClientMainParts : public content::BrowserMainParts {
 public:
  // Platform-specific create function.
  static std::unique_ptr<ViewsContentClientMainParts> Create(
      ViewsContentClient* views_content_client);

  static void PreBrowserMain();

  ViewsContentClientMainParts(const ViewsContentClientMainParts&) = delete;
  ViewsContentClientMainParts& operator=(const ViewsContentClientMainParts&) =
      delete;

  ~ViewsContentClientMainParts() override;

  // content::BrowserMainParts:
  int PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;

  content::ShellBrowserContext* browser_context() {
    return browser_context_.get();
  }

  ViewsContentClient* views_content_client() {
    return views_content_client_;
  }

 protected:
  explicit ViewsContentClientMainParts(
      ViewsContentClient* views_content_client);

#if BUILDFLAG(IS_APPLE)
  views::TestViewsDelegate* views_delegate() { return views_delegate_.get(); }
#endif

 private:
#if BUILDFLAG(IS_APPLE)
  display::ScopedNativeScreen desktop_screen_;
#endif

  std::unique_ptr<content::ShellBrowserContext> browser_context_;

  std::unique_ptr<views::TestViewsDelegate> views_delegate_;

  raw_ptr<ViewsContentClient> views_content_client_;

  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace ui

#endif  // UI_VIEWS_CONTENT_CLIENT_VIEWS_CONTENT_CLIENT_MAIN_PARTS_H_
