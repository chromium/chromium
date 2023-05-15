// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/test_ink_drop.h"
#include "ui/gfx/geometry/transform.h"

namespace views::test {

TestInkDrop::TestInkDrop() = default;
TestInkDrop::~TestInkDrop() = default;

void TestInkDrop::HostSizeChanged(const gfx::Size& new_size) {}

void TestInkDrop::HostViewThemeChanged() {}

void TestInkDrop::HostTransformChanged(const gfx::Transform& new_transform) {}

InkDropState TestInkDrop::GetTargetInkDropState() const {
  return state_;
}

void TestInkDrop::AnimateToState(InkDropState ink_drop_state) {
  state_ = ink_drop_state;
}

void TestInkDrop::SetHoverHighlightFadeDuration(base::TimeDelta duration_ms) {}

void TestInkDrop::UseDefaultHoverHighlightFadeDuration() {}

void TestInkDrop::SnapToActivated() {
  state_ = InkDropState::ACTIVATED;
}

void TestInkDrop::SnapToHidden() {
  state_ = InkDropState::HIDDEN;
}

void TestInkDrop::SetHovered(bool is_hovered) {
  is_hovered_ = is_hovered;
}

void TestInkDrop::SetFocused(bool is_focused) {}

bool TestInkDrop::IsHighlightFadingInOrVisible() const {
  return false;
}

void TestInkDrop::SetShowHighlightOnHover(bool show_highlight_on_hover) {}

void TestInkDrop::SetShowHighlightOnFocus(bool show_highlight_on_focus) {}

}  // namespace views::test
