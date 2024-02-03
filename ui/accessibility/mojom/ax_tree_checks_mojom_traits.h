// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_MOJOM_AX_TREE_CHECKS_MOJOM_TRAITS_H_
#define UI_ACCESSIBILITY_MOJOM_AX_TREE_CHECKS_MOJOM_TRAITS_H_

#include "ui/accessibility/ax_base_export.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_tree_checks.h"
#include "ui/accessibility/mojom/ax_tree_checks.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<ax::mojom::AXTreeChecksDataView, ui::AXTreeChecks> {
  static size_t node_count(const ui::AXTreeChecks& p) { return p.node_count; }

  static bool Read(ax::mojom::AXTreeChecksDataView data, ui::AXTreeChecks* out);
};

}  // namespace mojo

#endif  // UI_ACCESSIBILITY_MOJOM_AX_TREE_CHECKS_MOJOM_TRAITS_H_
