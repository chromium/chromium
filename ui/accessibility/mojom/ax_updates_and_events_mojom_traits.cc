// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_updates_and_events_mojom_traits.h"

#include "ui/accessibility/mojom/ax_event_mojom_traits.h"
#include "ui/accessibility/mojom/ax_tree_update_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    ax::mojom::AXUpdatesAndEventsDataView,
    ui::AXUpdatesAndEvents>::Read(ax::mojom::AXUpdatesAndEventsDataView data,
                                  ui::AXUpdatesAndEvents* out) {
  if (!data.ReadTreeId(&out->ax_tree_id)) {
    return false;
  }
  if (!data.ReadUpdates(&out->updates)) {
    return false;
  }
  if (!data.ReadEvents(&out->events)) {
    return false;
  }
  return true;
}

}  // namespace mojo
