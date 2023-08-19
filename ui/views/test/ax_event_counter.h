// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_AX_EVENT_COUNTER_H_
#define UI_VIEWS_TEST_AX_EVENT_COUNTER_H_

#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"

namespace views::test {

// AXEventCounter provides a convenient way to count events registered by the
// AXEventManager by their event type, and wait for events of a specific type.
class AXEventCounter : public views::AXEventObserver {
 public:
  explicit AXEventCounter(views::AXEventManager* event_manager);
  ~AXEventCounter() override;

  AXEventCounter(const AXEventCounter&) = delete;
  AXEventCounter& operator=(const AXEventCounter&) = delete;

  // Returns the number of events of a certain type registered since the
  // creation of this AXEventManager object and prior to the count being
  // manually reset.
  int GetCount(ax::mojom::Event event_type) const;

  // Returns the number of events of a certain type on a certain role registered
  // since the creation of this AXEventManager object and prior to the count
  // being manually reset.
  int GetCount(ax::mojom::Event event_type, ax::mojom::Role role) const;

  // Returns the number of events of a certain type on a specific view since the
  // creation of this AXEventManager object and prior to the count being
  // manually reset.
  int GetCount(ax::mojom::Event event_type, views::View* view) const;

  // Sets all counters to 0.
  void ResetAllCounts();

  // Blocks until an event of the specified type is received.
  void WaitForEvent(ax::mojom::Event event_type);

  // views::AXEventObserver
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override;

 private:
  mutable base::flat_map<ax::mojom::Event, int> event_counts_;
  mutable base::flat_map<std::pair<ax::mojom::Event, ax::mojom::Role>, int>
      event_counts_for_role_;
  mutable base::flat_map<std::pair<ax::mojom::Event, views::View*>, int>
      event_counts_for_view_;
  ax::mojom::Event wait_for_event_type_ = ax::mojom::Event::kNone;
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
  base::ScopedObservation<views::AXEventManager, views::AXEventObserver>
      tree_observation_{this};
};

}  // namespace views::test

#endif  // UI_VIEWS_TEST_AX_EVENT_COUNTER_H_
