// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
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
  ViewsContentClientMainPartsDesktopAura(
      const content::MainFunctionParams& content_params,
      ViewsContentClient* views_content_client);
  ~ViewsContentClientMainPartsDesktopAura() override {}

  // content::BrowserMainParts:
  void PreMainMessageLoopRun() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ViewsContentClientMainPartsDesktopAura);
};

ViewsContentClientMainPartsDesktopAura::ViewsContentClientMainPartsDesktopAura(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client)
    : ViewsContentClientMainPartsAura(content_params, views_content_client) {
}

void ViewsContentClientMainPartsDesktopAura::PreMainMessageLoopRun() {
  ViewsContentClientMainPartsAura::PreMainMessageLoopRun();

  display::Screen::SetScreenInstance(views::CreateDesktopScreen());

  views_content_client()->OnPreMainMessageLoopRun(browser_context(), nullptr);
}

}  // namespace

// static
std::unique_ptr<ViewsContentClientMainParts>
ViewsContentClientMainParts::Create(
    const content::MainFunctionParams& content_params,
    ViewsContentClient* views_content_client) {
  return std::make_unique<ViewsContentClientMainPartsDesktopAura>(
      content_params, views_content_client);
}

}  // namespace ui
