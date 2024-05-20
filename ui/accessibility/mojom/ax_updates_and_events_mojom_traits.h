// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_MOJOM_AX_UPDATES_AND_EVENTS_MOJOM_TRAITS_H_
#define UI_ACCESSIBILITY_MOJOM_AX_UPDATES_AND_EVENTS_MOJOM_TRAITS_H_

#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/accessibility/mojom/ax_updates_and_events.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<ax::mojom::AXUpdatesAndEventsDataView,
                    ui::AXUpdatesAndEvents> {
  static const ui::AXTreeID& tree_id(const ui::AXUpdatesAndEvents& p) {
    return p.ax_tree_id;
  }
  static const std::vector<ui::AXTreeUpdate>& updates(
      const ui::AXUpdatesAndEvents& p) {
    return p.updates;
  }
  static const std::vector<ui::AXEvent>& events(
      const ui::AXUpdatesAndEvents& p) {
    return p.events;
  }
  static bool Read(ax::mojom::AXUpdatesAndEventsDataView data,
                   ui::AXUpdatesAndEvents* out);
};

}  // namespace mojo

#endif  // UI_ACCESSIBILITY_MOJOM_AX_UPDATES_AND_EVENTS_MOJOM_TRAITS_H_
