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

TestWindowBuilder::TestWindowBuilder() = default;

TestWindowBuilder::TestWindowBuilder(TestWindowBuilder& others)
    : parent_(others.parent_),
      delegate_(others.delegate_),
      window_type_(others.window_type_),
      layer_type_(others.layer_type_),
      bounds_(others.bounds_),
      init_properties_(std::move(others.init_properties_)),
      window_id_(others.window_id_),
      window_title_(others.window_title_),
      show_(others.show_) {
  DCHECK(!others.built_);
  others.built_ = true;
}

TestWindowBuilder::~TestWindowBuilder() = default;

TestWindowBuilder& TestWindowBuilder::SetParent(Window* parent) {
  DCHECK(!built_);
  DCHECK(!parent_);
  parent_ = parent;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetWindowType(client::WindowType type) {
  DCHECK(!built_);
  window_type_ = type;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetWindowId(int id) {
  DCHECK(!built_);
  window_id_ = id;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetWindowTitle(
    const std::u16string& title) {
  DCHECK(!built_);
  window_title_ = title;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetBounds(const gfx::Rect& bounds) {
  DCHECK(!built_);
  bounds_ = bounds;
  return *this;
}

TestWindowBuilder& TestWindowBuilder::SetDelegate(WindowDelegate* delegate) {
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
  show_ = show;
  return *this;
}

std::unique_ptr<Window> TestWindowBuilder::Build() {
  auto window = CreateWindowInternal();
  if (parent_) {
    if (!bounds_.IsEmpty()) {
      window->SetBounds(bounds_);
    }
    parent_->AddChild(window.get());
  } else {
    // Parent window is not specified. A parent window will be picked from
    // the context.
    aura::Window* context = window.get()->GetRootWindow();
    CHECK(context);
    client::ParentWindowWithContext(window.get(), context, bounds_,
                                    display::kInvalidDisplayId);
  }
  if (show_) {
    window->Show();
  }
  return window;
}

std::unique_ptr<Window> TestWindowBuilder::CreateWindowInternal() {
  DCHECK(!built_);
  built_ = true;
  std::unique_ptr<Window> window =
      std::make_unique<Window>(delegate_, window_type_);
  window->Init(layer_type_);
  window->AcquireAllPropertiesFrom(std::move(init_properties_));
  if (window_id_ != Window::kInitialId) {
    window->SetId(window_id_);
  }
  if (!window_title_.empty()) {
    window->SetTitle(window_title_);
  }
  return window;
}

}  // namespace aura::test
