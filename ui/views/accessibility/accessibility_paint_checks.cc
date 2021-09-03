// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/accessibility_paint_checks.h"

#include <string>

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"

namespace views {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSkipAccessibilityPaintChecks, false)

namespace {

std::string GetViewTreeAsString(View* view) {
  if (!view->parent())
    return view->GetClassName();
  return GetViewTreeAsString(view->parent()) + " -> " + view->GetClassName();
}

}  // namespace

void RunAccessibilityPaintChecks(View* view) {
  if (view->GetProperty(kSkipAccessibilityPaintChecks))
    return;
  ui::AXNodeData node_data;
  view->GetViewAccessibility().GetAccessibleNodeData(&node_data);

  if (!node_data.HasState(ax::mojom::State::kFocusable))
    return;

  // Focusable nodes must have an accessible name, otherwise screen reader users
  // will not know what they landed on. For example, the reload button should
  // have an accessible name of "Reload".
  // Exceptions:
  // 1) Textfields can set the placeholder string attribute.
  // 2) Explicitly setting the name to "" is allowed if the view uses
  // AXNodedata.SetNameExplicitlyEmpty().

  // It has a name, we're done.
  if (!node_data.GetStringAttribute(ax::mojom::StringAttribute::kName).empty())
    return;

  // Text fields are allowed to have a placeholder instead.
  if (node_data.role == ax::mojom::Role::kTextField &&
      !node_data.GetStringAttribute(ax::mojom::StringAttribute::kPlaceholder)
           .empty()) {
    return;
  }

  // Finally, a view is allowed to explicitly state that it has no name. Note
  // that while this is a CHECK, calling code may decide to only run this if
  // DCHECKs are enabled.
  CHECK_EQ(node_data.GetNameFrom(),
           ax::mojom::NameFrom::kAttributeExplicitlyEmpty)
      << " " << view << ": " << view->GetClassName()
      << " is focusable but has no accessible name or placeholder, and is not "
         "explicitly marked as empty.\n"
      << GetViewTreeAsString(view);
}

}  // namespace views
