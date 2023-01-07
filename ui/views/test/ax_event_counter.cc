// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ui/views/test/ax_event_counter.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/views/view.h"

namespace views {
namespace test {

AXEventCounter::AXEventCounter(views::AXEventManager* event_manager) {
  tree_observation_.Observe(event_manager);
}

AXEventCounter::~AXEventCounter() = default;

void AXEventCounter::OnViewEvent(views::View* view,
                                 ax::mojom::Event event_type) {
  ++event_counts_[event_type];

  // TODO(accessibility): There are a non-trivial number of events, mostly
  // kChildrenChanged, being fired during the creation process. When this
  // occurs calling GetAccessibleNodeData() on the related View can lead
  // to null dereference errors and at least one pure-virtual-function call.
  // We should either fix those errors or stop firing the events. For now,
  // require the presence of a Widget to count events by role.
  if (view->GetWidget()) {
    ui::AXNodeData node_data;
    view->GetAccessibleNodeData(&node_data);
    ++event_counts_for_role_[std::make_pair(event_type, node_data.role)];
  }

  if (run_loop_ && event_type == wait_for_event_type_) {
    wait_for_event_type_ = ax::mojom::Event::kNone;
    run_loop_->Quit();
  }
}

int AXEventCounter::GetCount(ax::mojom::Event event_type) {
  return event_counts_[event_type];
}

int AXEventCounter::GetCount(ax::mojom::Event event_type,
                             ax::mojom::Role role) {
  return event_counts_for_role_[std::make_pair(event_type, role)];
}

void AXEventCounter::ResetAllCounts() {
  event_counts_.clear();
  event_counts_for_role_.clear();
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
