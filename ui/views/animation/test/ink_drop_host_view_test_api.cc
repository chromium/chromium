// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/ink_drop_host_view_test_api.h"

namespace views {
namespace test {

InkDropHostViewTestApi::InkDropHostViewTestApi(InkDropHostView* host_view)
    : host_view_(host_view) {}

InkDropHostViewTestApi::~InkDropHostViewTestApi() = default;

void InkDropHostViewTestApi::SetInkDropMode(InkDropMode ink_drop_mode) {
  host_view_->SetInkDropMode(ink_drop_mode);
}

void InkDropHostViewTestApi::SetInkDrop(std::unique_ptr<InkDrop> ink_drop,
                                        bool handles_gesture_events) {
  host_view_->SetInkDropMode(handles_gesture_events
                                 ? InkDropMode::ON
                                 : InkDropMode::ON_NO_GESTURE_HANDLER);
  host_view_->ink_drop_ = std::move(ink_drop);
}

void InkDropHostViewTestApi::SetInkDrop(std::unique_ptr<InkDrop> ink_drop) {
  SetInkDrop(std::move(ink_drop), true);
}

bool InkDropHostViewTestApi::HasInkDrop() const {
  return host_view_->HasInkDrop();
}

InkDrop* InkDropHostViewTestApi::GetInkDrop() {
  return host_view_->GetInkDrop();
}

gfx::Point InkDropHostViewTestApi::GetInkDropCenterBasedOnLastEvent() const {
  return host_view_->GetInkDropCenterBasedOnLastEvent();
}

void InkDropHostViewTestApi::AnimateInkDrop(InkDropState state,
                                            const ui::LocatedEvent* event) {
  host_view_->AnimateInkDrop(state, event);
}

}  // namespace test
}  // namespace views
