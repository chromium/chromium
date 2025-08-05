// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_window_builder.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace aura::test {

TestWindowBuilder::TestWindowBuilder(WindowBuilderParams params)
    : params_(std::move(params)) {}

TestWindowBuilder::TestWindowBuilder(TestWindowBuilder& others)
    : params_(std::move(others.params_)),
      delegate_(std::move(others.delegate_)),
      init_properties_(std::move(others.init_properties_)) {
  DCHECK(!others.built_);
  others.built_ = true;
}

TestWindowBuilder::~TestWindowBuilder() = default;

TestWindowBuilder& TestWindowBuilder::SetParent(Window* parent) {
  DCHECK(!built_);
  DCHECK(!params_.parent);
  params_.parent = parent;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetWindowType(client::WindowType type) {
  DCHECK(!built_);
  params_.window_type = type;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetWindowId(int id) {
  DCHECK(!built_);
  params_.window_id = id;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetWindowTitle(
    const std::u16string& title) {
  DCHECK(!built_);
  params_.window_title = title;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetBounds(const gfx::Rect& bounds) {
  DCHECK(!built_);
  params_.bounds = bounds;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetDelegate(WindowDelegate* delegate) {
  DCHECK(!built_);
  DCHECK(!delegate_);
  delegate_ = delegate;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetColorWindowDelegate(SkColor color) {
  DCHECK(!built_);
  DCHECK(!delegate_);
  delegate_ = new ColorTestWindowDelegate(color);
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetTestWindowDelegate() {
  DCHECK(!built_);
  DCHECK(!delegate_);
  delegate_ = TestWindowDelegate::CreateSelfDestroyingDelegate();
  return *this;
}

TestWindowBuilder& TestWindowBuilder::AllowAllWindowStates() {
  DCHECK(!built_);
  init_properties_.SetProperty(client::kResizeBehaviorKey,
                               client::kResizeBehaviorCanFullscreen |
                                   client::kResizeBehaviorCanMaximize |
                                   client::kResizeBehaviorCanMinimize |
                                   client::kResizeBehaviorCanResize);
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetShow(bool show) {
  DCHECK(!built_);
  params_.show = show;
  return *this;
}

std::unique_ptr<Window> TestWindowBuilder::Build() {
  auto window = CreateWindowInternal();
  if (parent()) {
    if (!params_.bounds.IsEmpty()) {
      window->SetBounds(params_.bounds);
    }
    params_.parent->AddChild(window.get());
  } else {
    // Parent window is not specified. A parent window will be picked from
    // the context.
    aura::Window* context = window.get()->GetRootWindow();
    CHECK(context);
    client::ParentWindowWithContext(window.get(), context, params_.bounds,
                                    display::kInvalidDisplayId);
  }
  if (params_.show) {
    window->Show();
  }
  return window;
}

std::unique_ptr<Window> TestWindowBuilder::CreateWindowInternal() {
  DCHECK(!built_);
  built_ = true;
  std::unique_ptr<Window> window =
      std::make_unique<Window>(delegate_, params_.window_type);
  delegate_ = nullptr;
  window->Init(params_.layer_type);
  window->AcquireAllPropertiesFrom(std::move(init_properties_));
  if (params_.window_id != Window::kInitialId) {
    window->SetId(params_.window_id);
  }
  if (!params_.window_title.empty()) {
    window->SetTitle(params_.window_title);
  }
  return window;
}

}  // namespace aura::test
