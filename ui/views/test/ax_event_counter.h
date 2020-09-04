// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_AX_EVENT_COUNTER_H_
#define UI_VIEWS_TEST_AX_EVENT_COUNTER_H_

#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"

namespace views {
namespace test {

// AXEventCounter provides a convenient way to count events registered by the
// AXEventManager by their event type, and wait for events of a specific type.
class AXEventCounter : public views::AXEventObserver {
 public:
  explicit AXEventCounter(views::AXEventManager* event_manager);
  ~AXEventCounter() override;

  AXEventCounter(const AXEventCounter&) = delete;
  AXEventCounter& operator=(const AXEventCounter&) = delete;

  // Returns the number of events of a certain type registered since the
  // creation of this AXEventManager object.
  int GetCount(ax::mojom::Event event_type);

  // Blocks until an event of the specified type is received.
  void WaitForEvent(ax::mojom::Event event_type);

  // views::AXEventObserver
  void OnViewEvent(views::View*, ax::mojom::Event event_type) override;

 private:
  base::flat_map<ax::mojom::Event, int> event_counts_;
  ax::mojom::Event wait_for_event_type_ = ax::mojom::Event::kNone;
  base::RunLoop* run_loop_ = nullptr;
  ScopedObserver<views::AXEventManager, views::AXEventObserver> tree_observer_{
      this};
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_AX_EVENT_COUNTER_H_
