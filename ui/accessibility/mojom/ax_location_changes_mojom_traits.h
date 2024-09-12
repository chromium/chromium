// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_MOJOM_AX_LOCATION_CHANGES_MOJOM_TRAITS_H_
#define UI_ACCESSIBILITY_MOJOM_AX_LOCATION_CHANGES_MOJOM_TRAITS_H_

#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/accessibility/mojom/ax_location_changes.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<ax::mojom::AXLocationChangesDataView,
                    ui::AXLocationChanges> {
  static int32_t id(const ui::AXLocationChanges& p) { return p.id; }
  static const ui::AXTreeID& ax_tree_id(const ui::AXLocationChanges& p) {
    return p.ax_tree_id;
  }
  static const ui::AXRelativeBounds& new_location(
      const ui::AXLocationChanges& p) {
    return p.new_location;
  }
  static bool Read(ax::mojom::AXLocationChangesDataView data,
                   ui::AXLocationChanges* out);
};

}  // namespace mojo

#endif  // UI_ACCESSIBILITY_MOJOM_AX_LOCATION_CHANGES_MOJOM_TRAITS_H_
