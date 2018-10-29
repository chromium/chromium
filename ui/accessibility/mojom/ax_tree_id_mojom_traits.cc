// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_tree_id_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<ax::mojom::AXTreeIDDataView, ui::AXTreeID>::Read(
    ax::mojom::AXTreeIDDataView data,
    ui::AXTreeID* out) {
  if (!data.ReadId(&out->id_))
    return false;
  return true;
}

}  // namespace mojo
