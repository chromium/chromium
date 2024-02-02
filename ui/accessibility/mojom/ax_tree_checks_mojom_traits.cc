// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_tree_checks_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<ax::mojom::AXTreeChecksDataView, ui::AXTreeChecks>::Read(
    ax::mojom::AXTreeChecksDataView data,
    ui::AXTreeChecks* out) {
  out->node_count = data.node_count();
  return true;
}

}  // namespace mojo
