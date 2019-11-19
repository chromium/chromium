// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "content/public/browser/context_factory.h"
#include "content/shell/browser/shell_browser_context.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/views_content_client/views_content_client.h"
#include "ui/views_content_client/views_content_client_main_parts_aura.h"
#include "ui/wm/test/wm_test_helper.h"

namespace ui {

namespace {

class ViewsContentClientMainPartsChromeOS
    : public ViewsContentClientMainPartsAura {
 public:
  ViewsContentClientMainPartsChromeOS(
      const content::MainFunctionParams& content_params,
      ViewsContentClient* views_content_client);
  ~ViewsContentClientMainPartsChromeOS() override {}

  // content::BrowserMainParts:
  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;

 private:
  // Enable a minimal set of views::corewm to be initialized.
  std::unique_ptr<display::Screen> test_screen_;
  std::unique_ptr<::wm::WMTestHelper> wm_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(ViewsContentClientMainPartsChromeOS);
};

ViewsContentClientMainPartsChromeOS::ViewsContentClientMainPartsChromeOS(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client)
    : ViewsContentClientMainPartsAura(content_params, views_content_client) {
}

void ViewsContentClientMainPartsChromeOS::PreMainMessageLoopRun() {
  ViewsContentClientMainPartsAura::PreMainMessageLoopRun();

  gfx::Size host_size(800, 600);
  test_screen_.reset(aura::TestScreen::Create(host_size));
  display::Screen::SetScreenInstance(test_screen_.get());
  // Set up basic pieces of views::corewm.
  wm_test_helper_ = std::make_unique<wm::WMTestHelper>(host_size);
  // Ensure the X window gets mapped.
  wm_test_helper_->host()->Show();

  // Ensure Aura knows where to open new windows.
  aura::Window* root_window = wm_test_helper_->host()->window();
  views_content_client()->OnPreMainMessageLoopRun(browser_context(),
                                                  root_window);
}

void ViewsContentClientMainPartsChromeOS::PostMainMessageLoopRun() {
  wm_test_helper_.reset();
  test_screen_.reset();

  ViewsContentClientMainPartsAura::PostMainMessageLoopRun();
}

}  // namespace

// static
std::unique_ptr<ViewsContentClientMainParts>
ViewsContentClientMainParts::Create(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client) {
  return std::make_unique<ViewsContentClientMainPartsChromeOS>(
      content_params, views_content_client);
}

}  // namespace ui
