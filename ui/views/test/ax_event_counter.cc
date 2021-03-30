// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/ax_event_counter.h"

namespace views {
namespace test {

AXEventCounter::AXEventCounter(views::AXEventManager* event_manager) {
  tree_observation_.Observe(event_manager);
}

AXEventCounter::~AXEventCounter() = default;

void AXEventCounter::OnViewEvent(views::View*, ax::mojom::Event event_type) {
  ++event_counts_[event_type];
  if (run_loop_ && event_type == wait_for_event_type_) {
    wait_for_event_type_ = ax::mojom::Event::kNone;
    run_loop_->Quit();
  }
}

int AXEventCounter::GetCount(ax::mojom::Event event_type) {
  return event_counts_[event_type];
}

void AXEventCounter::WaitForEvent(ax::mojom::Event event_type) {
  wait_for_event_type_ = event_type;
  base::RunLoop run_loop;
  run_loop_ = &run_loop;
  run_loop_->Run();
  run_loop_ = nullptr;
}

}  // namespace test
}  // namespace views
