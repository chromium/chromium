// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_location_changes_mojom_traits.h"

#include "ui/accessibility/mojom/ax_relative_bounds_mojom_traits.h"
#include "ui/accessibility/mojom/ax_tree_update_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<ax::mojom::AXLocationChangesDataView, ui::AXLocationChanges>::
    Read(ax::mojom::AXLocationChangesDataView data,
         ui::AXLocationChanges* out) {
  out->id = data.id();
  if (!data.ReadAxTreeId(&out->ax_tree_id)) {
    return false;
  }
  if (!data.ReadNewLocation(&out->new_location)) {
    return false;
  }
  return true;
}

}  // namespace mojo
