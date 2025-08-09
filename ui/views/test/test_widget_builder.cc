// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_widget_builder.h"

#include <utility>

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace views::test {

TestWidgetBuilder::TestWidgetBuilder(WidgetBuilderParams params)
    : params_(std::move(params)) {}

TestWidgetBuilder::TestWidgetBuilder(TestWidgetBuilder&& other)
    : widget_init_params_(std::move(other.widget_init_params_)),
      params_(std::move(other.params_)) {
  DCHECK(!other.built_);
  other.built_ = true;
}

TestWidgetBuilder::~TestWidgetBuilder() = default;

TestWidgetBuilder& TestWidgetBuilder::SetWidgetType(
    Widget::InitParams::Type type) {
  DCHECK(!built_);
  widget_init_params_.type = type;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetZOrderLevel(ui::ZOrderLevel z_order) {
  DCHECK(!built_);
  widget_init_params_.z_order = z_order;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetDelegate(WidgetDelegate* delegate) {
  DCHECK(!built_);
  DCHECK(!widget_init_params_.delegate);
  widget_init_params_.delegate = delegate;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetBounds(const gfx::Rect& bounds) {
  DCHECK(!built_);
  widget_init_params_.bounds = bounds;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetParent(aura::Window* parent) {
  DCHECK(!built_);
  widget_init_params_.parent = parent;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetContext(aura::Window* context) {
  DCHECK(!built_);
  widget_init_params_.context = context;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetActivatable(bool activatable) {
  DCHECK(!built_);
  widget_init_params_.activatable = activatable
                                        ? Widget::InitParams::Activatable::kYes
                                        : Widget::InitParams::Activatable::kNo;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetShowState(
    ui::mojom::WindowShowState show_state) {
  DCHECK(!built_);
  widget_init_params_.show_state = show_state;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetWindowId(int window_id) {
  DCHECK(!built_);
  params_.window_id = window_id;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetWindowTitle(
    const std::u16string& title) {
  DCHECK(!built_);
  params_.window_title = title;
  return *this;
}

TestWidgetBuilder& TestWidgetBuilder::SetShow(bool show) {
  DCHECK(!built_);
  params_.show = show;
  return *this;
}

std::unique_ptr<Widget> TestWidgetBuilder::BuildOwnsNativeWidget() {
  return BuildWidgetWithOwnership(
      Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
}

std::unique_ptr<Widget> TestWidgetBuilder::BuildClientOwnsWidget() {
  return BuildWidgetWithOwnership(Widget::InitParams::CLIENT_OWNS_WIDGET);
}

std::unique_ptr<Widget> TestWidgetBuilder::BuildWidgetWithOwnership(
    Widget::InitParams::Ownership ownership) {
  DCHECK(!built_);
  built_ = true;

  std::unique_ptr<Widget> widget = std::make_unique<Widget>();
  widget_init_params_.ownership = ownership;
  widget->Init(std::move(widget_init_params_));
  if (params_.window_id != aura::Window::kInitialId) {
    widget->GetNativeWindow()->SetId(params_.window_id);
  }
  if (!params_.window_title.empty()) {
    widget->GetNativeWindow()->SetTitle(params_.window_title);
  }
  if (params_.show) {
    widget->Show();
  }
  return widget;
}

Widget* TestWidgetBuilder::BuildOwnedByNativeWidget() {
  DCHECK(!built_);
  built_ = true;

  Widget* widget = new Widget();
  widget_init_params_.ownership = Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
  widget->Init(std::move(widget_init_params_));

  if (params_.window_id != aura::Window::kInitialId) {
    widget->GetNativeWindow()->SetId(params_.window_id);
  }
  if (!params_.window_title.empty()) {
    widget->GetNativeWindow()->SetTitle(params_.window_title);
  }
  if (params_.show) {
    widget->Show();
  }
  return widget;
}

}  // namespace views::test
