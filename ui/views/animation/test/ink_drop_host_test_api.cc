// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/test/ink_drop_host_test_api.h"

#include <utility>

#include "ui/views/animation/ink_drop_mask.h"

namespace views::test {

InkDropHostTestApi::InkDropHostTestApi(InkDropHost* ink_drop_host)
    : ink_drop_host_(*ink_drop_host) {}

InkDropHostTestApi::~InkDropHostTestApi() = default;

void InkDropHostTestApi::SetInkDropMode(InkDropMode ink_drop_mode) {
  ink_drop_host_->SetMode(ink_drop_mode);
}

void InkDropHostTestApi::SetInkDrop(std::unique_ptr<InkDrop> ink_drop,
                                    bool handles_gesture_events) {
  ink_drop_host_->SetMode(
      handles_gesture_events
          ? views::InkDropHost::InkDropMode::ON
          : views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
  ink_drop_host_->ink_drop_ = std::move(ink_drop);
}

void InkDropHostTestApi::SetInkDrop(std::unique_ptr<InkDrop> ink_drop) {
  SetInkDrop(std::move(ink_drop), true);
}

bool InkDropHostTestApi::HasInkDrop() const {
  return ink_drop_host_->HasInkDrop();
}

InkDrop* InkDropHostTestApi::GetInkDrop() {
  return ink_drop_host_->GetInkDrop();
}

void InkDropHostTestApi::AnimateToState(InkDropState state,
                                        const ui::LocatedEvent* event) {
  ink_drop_host_->AnimateToState(state, event);
}

void InkDropHostTestApi::RemoveInkDropMask() {
  ink_drop_host_->ink_drop_mask_.reset();
}

}  // namespace views::test
