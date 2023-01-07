// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/result_codes.h"
#include "content/shell/browser/shell_browser_context.h"
#include "ui/display/screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#include "ui/views_content_client/views_content_client.h"
#include "ui/views_content_client/views_content_client_main_parts_aura.h"

namespace ui {

namespace {

class ViewsContentClientMainPartsDesktopAura
    : public ViewsContentClientMainPartsAura {
 public:
  explicit ViewsContentClientMainPartsDesktopAura(
      ViewsContentClient* views_content_client);
  ViewsContentClientMainPartsDesktopAura(
      const ViewsContentClientMainPartsDesktopAura&) = delete;
  ViewsContentClientMainPartsDesktopAura& operator=(
      const ViewsContentClientMainPartsDesktopAura&) = delete;
  ~ViewsContentClientMainPartsDesktopAura() override = default;

  // ViewsContentClientMainPartsAura:
  int PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;

 private:
  std::unique_ptr<display::Screen> screen_;
};

ViewsContentClientMainPartsDesktopAura::ViewsContentClientMainPartsDesktopAura(
    ViewsContentClient* views_content_client)
    : ViewsContentClientMainPartsAura(views_content_client) {}

int ViewsContentClientMainPartsDesktopAura::PreMainMessageLoopRun() {
  ViewsContentClientMainPartsAura::PreMainMessageLoopRun();

  screen_ = views::CreateDesktopScreen();

  views_content_client()->OnPreMainMessageLoopRun(browser_context(), nullptr);

  return content::RESULT_CODE_NORMAL_EXIT;
}

void ViewsContentClientMainPartsDesktopAura::PostMainMessageLoopRun() {
  screen_.reset();

  ViewsContentClientMainPartsAura::PostMainMessageLoopRun();
}

}  // namespace

// static
std::unique_ptr<ViewsContentClientMainParts>
ViewsContentClientMainParts::Create(ViewsContentClient* views_content_client) {
  return std::make_unique<ViewsContentClientMainPartsDesktopAura>(
      views_content_client);
}

}  // namespace ui
