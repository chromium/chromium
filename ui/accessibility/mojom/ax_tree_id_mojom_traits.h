// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_MOJOM_AX_TREE_ID_MOJOM_TRAITS_H_
#define UI_ACCESSIBILITY_MOJOM_AX_TREE_ID_MOJOM_TRAITS_H_

#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/mojom/ax_tree_id.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<ax::mojom::AXTreeIDDataView, ui::AXTreeID> {
  static const std::string& id(const ui::AXTreeID& p) { return p.ToString(); }

  static bool Read(ax::mojom::AXTreeIDDataView data, ui::AXTreeID* out);
};

}  // namespace mojo

#endif  // UI_ACCESSIBILITY_MOJOM_AX_TREE_ID_MOJOM_TRAITS_H_
