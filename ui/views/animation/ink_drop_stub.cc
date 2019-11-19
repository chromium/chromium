// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_stub.h"

namespace views {

InkDropStub::InkDropStub() = default;

InkDropStub::~InkDropStub() = default;

void InkDropStub::HostSizeChanged(const gfx::Size& new_size) {}

InkDropState InkDropStub::GetTargetInkDropState() const {
  return InkDropState::HIDDEN;
}

void InkDropStub::AnimateToState(InkDropState state) {}

void InkDropStub::SetHoverHighlightFadeDuration(base::TimeDelta duration) {}

void InkDropStub::UseDefaultHoverHighlightFadeDuration() {}

void InkDropStub::SnapToActivated() {}

void InkDropStub::SnapToHidden() {}

void InkDropStub::SetHovered(bool is_hovered) {}

void InkDropStub::SetFocused(bool is_hovered) {}

bool InkDropStub::IsHighlightFadingInOrVisible() const {
  return false;
}

void InkDropStub::SetShowHighlightOnHover(bool show_highlight_on_hover) {}

void InkDropStub::SetShowHighlightOnFocus(bool show_highlight_on_focus) {}

}  // namespace views
