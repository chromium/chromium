// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/accessibility_paint_checks.h"

#include <string>

#include "build/chromeos_buildflags.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/class_property.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace views {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kSkipAccessibilityPaintChecks, false)

void RunAccessibilityPaintChecks(View* view) {
  // Note that none of these checks run if DCHECKs are off. Dead-code
  // elimination should remove the following. This is done instead of #ifs to
  // make sure that the code compiles regardless of DCHECK availability.
  if (!DCHECK_IS_ON())
    return;

  if (view->GetProperty(kSkipAccessibilityPaintChecks))
    return;

  // Get accessible node data from ViewAccessibility instead of View, because
  // some additional fields are processed and set there.
  ui::AXNodeData node_data;
  view->GetViewAccessibility().GetAccessibleNodeData(&node_data);

  // No checks for unfocusable items yet.
  if (!node_data.HasState(ax::mojom::State::kFocusable))
    return;

// TODO(crbug.com/40185544): Enable these DCHECKs on ash. One of the current
// failures seem to be SearchResultPageView marking itself as ignored
// (temporarily), which marks focusable children as ignored. One way of enabling
// these here would be to turn `kSkipAccessibilityPaintChecks` into a cascading
// property or introduce a cascading property specifically for the current
// misbehavior in SearchResultPageView to be able to suppress that and enable
// the DCHECK elsewhere.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(!node_data.HasState(ax::mojom::State::kIgnored))
      << "View is focusable and should not be ignored.\n"
      << GetViewDebugInfo(view);

  DCHECK(!node_data.IsInvisible())
      << "View is focusable and should not be invisible.\n"
      << GetViewDebugInfo(view);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // ViewAccessibility::GetAccessibleNodeData currently returns early, after
  // setting the role to kUnknown, the NameFrom to kAttributeExplicitlyEmpty,
  // and adding the kDisabled restriction, if the Widget is closed.
  if (node_data.GetRestriction() != ax::mojom::Restriction::kDisabled) {
    // Focusable views should have a valid role.
    DCHECK(node_data.role != ax::mojom::Role::kNone &&
           node_data.role != ax::mojom::Role::kUnknown)
        << "View is focusable but lacks a valid role.\n"
        << GetViewDebugInfo(view);
  }

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

  // Finally, a view is allowed to explicitly state that it has no name.
  DCHECK_EQ(node_data.GetNameFrom(),
            ax::mojom::NameFrom::kAttributeExplicitlyEmpty)
      << "View is focusable but has no accessible name or placeholder, and is "
         "not explicitly marked as empty. The accessible name is spoken by "
         "screen readers to end users. Thus if this is production code, the "
         "accessible name should be localized.\n"
      << GetViewDebugInfo(view);
}

void RunAccessibilityPaintChecksRecursive(View* view) {
  RunAccessibilityPaintChecks(view);
  for (views::View* v : view->children()) {
    RunAccessibilityPaintChecksRecursive(v);
  }
}

void RunAccessibilityPaintChecks(Widget* widget) {
  RunAccessibilityPaintChecksRecursive(widget->GetRootView());
}

}  // namespace views
