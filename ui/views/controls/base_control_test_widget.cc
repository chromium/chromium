// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/base_control_test_widget.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "ui/display/mac/test/test_screen_mac.h"
#include "ui/display/screen.h"
#endif

namespace views::test {

BaseControlTestWidget::BaseControlTestWidget() = default;
BaseControlTestWidget::~BaseControlTestWidget() = default;

void BaseControlTestWidget::SetUp() {
#if BUILDFLAG(IS_MAC)
  test_screen_ = std::make_unique<display::test::TestScreenMac>(gfx::Size());
  // Purposely not use ScopedScreenOverride, in which GetScreen() will
  // create a native screen.
  display::Screen::SetScreenInstance(test_screen_.get());
#endif

  ViewsTestBase::SetUp();

  widget_ = std::make_unique<Widget>();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                   Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(200, 200);
  widget_->Init(std::move(params));
  auto* container = widget_->SetContentsView(std::make_unique<View>());

  CreateWidgetContent(container);

  widget_->Show();
}

void BaseControlTestWidget::TearDown() {
  widget_.reset();

  ViewsTestBase::TearDown();
#if BUILDFLAG(IS_MAC)
  display::Screen::SetScreenInstance(nullptr);
#endif
}

void BaseControlTestWidget::CreateWidgetContent(View* container) {}

}  // namespace views::test
