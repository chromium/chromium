// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_location_and_scroll_updates_mojom_traits.h"

#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/mojom/ax_location_and_scroll_updates.mojom-shared.h"
#include "ui/accessibility/mojom/ax_relative_bounds_mojom_traits.h"
#include "ui/accessibility/mojom/ax_tree_update_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<ax::mojom::AXLocationChangeDataView, ui::AXLocationChange>::
    Read(ax::mojom::AXLocationChangeDataView data, ui::AXLocationChange* out) {
  out->id = data.id();
  if (!data.ReadNewLocation(&out->new_location)) {
    return false;
  }
  return true;
}

bool StructTraits<ax::mojom::AXScrollChangeDataView, ui::AXScrollChange>::Read(
    ax::mojom::AXScrollChangeDataView data,
    ui::AXScrollChange* out) {
  out->id = data.id();
  out->scroll_x = data.scroll_x();
  out->scroll_y = data.scroll_y();
  return true;
}

bool StructTraits<ax::mojom::AXLocationAndScrollUpdatesDataView,
                  ui::AXLocationAndScrollUpdates>::
    Read(ax::mojom::AXLocationAndScrollUpdatesDataView data,
         ui::AXLocationAndScrollUpdates* out) {
  if (!data.ReadLocationChanges(&out->location_changes)) {
    return false;
  }
  if (!data.ReadScrollChanges(&out->scroll_changes)) {
    return false;
  }
  return true;
}

}  // namespace mojo
