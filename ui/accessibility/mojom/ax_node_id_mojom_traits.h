// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_MOJOM_AX_NODE_ID_MOJOM_TRAITS_H_
#define UI_ACCESSIBILITY_MOJOM_AX_NODE_ID_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/mojom/ax_node_id.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<ax::mojom::AXNodeIDDataView, ui::AXNodeID> {
  static int32_t value(ui::AXNodeID id) { return id; }
  static bool Read(ax::mojom::AXNodeIDDataView data, ui::AXNodeID* out) {
    *out = data.value();
    return true;
  }
};

}  // namespace mojo

#endif  // UI_ACCESSIBILITY_MOJOM_AX_NODE_ID_MOJOM_TRAITS_H_
