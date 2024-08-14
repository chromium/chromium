// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_accessibility_utils.h"

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

// static
Widget* ViewAccessibilityUtils::GetFocusedChildWidgetForAccessibility(
    const View* view) {
  const FocusManager* focus_manager = view->GetFocusManager();
  if (!focus_manager)
    return nullptr;
  const View* focused_view = view->GetFocusManager()->GetFocusedView();
  if (!focused_view)
    return nullptr;

  std::set<raw_ptr<Widget, SetExperimental>> child_widgets;
  Widget::GetAllOwnedWidgets(view->GetWidget()->GetNativeView(),
                             &child_widgets);
  const auto i = base::ranges::find_if(
      child_widgets, [focused_view](Widget* child_widget) {
        return IsFocusedChildWidget(child_widget, focused_view);
      });
  return (i == child_widgets.cend()) ? nullptr : *i;
}

// static
bool ViewAccessibilityUtils::IsFocusedChildWidget(Widget* widget,
                                                  const View* focused_view) {
  return widget->IsVisible() &&
         widget->GetContentsView()->Contains(focused_view);
}

// static
void ViewAccessibilityUtils::Merge(const ui::AXNodeData& source,
                                   ui::AXNodeData& destination) {
  if (source.role != ax::mojom::Role::kUnknown) {
    destination.role = source.role;
  }

  for (const auto& attr : source.int_attributes) {
    destination.AddIntAttribute(attr.first, attr.second);
  }

  for (const auto& attr : source.string_attributes) {
    // Child Tree ID attribute must be added using AddChildTreeId, otherwise
    // DCHECK will be hit.
    if (attr.first == ax::mojom::StringAttribute::kChildTreeId) {
      destination.AddChildTreeId(ui::AXTreeID::FromString(attr.second));
    } else {
      destination.AddStringAttribute(attr.first, attr.second);
    }
  }

  for (const auto& attr : source.bool_attributes) {
    destination.AddBoolAttribute(attr.first, attr.second);
  }

  for (const auto& attr : source.intlist_attributes) {
    destination.AddIntListAttribute(attr.first, attr.second);
  }

  for (const auto& attr : source.stringlist_attributes) {
    destination.AddStringListAttribute(attr.first, attr.second);
  }

  for (const auto& attr : source.float_attributes) {
    destination.AddFloatAttribute(attr.first, attr.second);
  }

  if (!source.relative_bounds.bounds.IsEmpty()) {
    destination.relative_bounds.bounds = source.relative_bounds.bounds;
  }

  destination.state |= source.state;

  destination.actions |= source.actions;
}

// static
void ViewAccessibilityUtils::ValidateAttributesNotSet(
    const ui::AXNodeData& new_data,
    const ui::AXNodeData& existing_data) {
  auto attributeErrorMessage = [](std::string attr) -> std::string {
    return "The \"" + attr +
           "\" attribute has been migrated to use the new AXNodeData pipeline. "
           "Please use "
           "the setters/getters in ViewAccessibility to set the attribute."
           "See the comment in ViewAccessibility::GetAccessibleNodeData for "
           "more info.";
  };

  for (const auto& attr : new_data.int_attributes) {
    DCHECK(!existing_data.HasIntAttribute(attr.first))
        << attributeErrorMessage(std::string(ui::ToString(attr.first)));
  }

  for (const auto& attr : new_data.string_attributes) {
    DCHECK(!existing_data.HasStringAttribute(attr.first))
        << attributeErrorMessage(std::string(ui::ToString(attr.first)));
  }

  for (const auto& attr : new_data.bool_attributes) {
    DCHECK(!existing_data.HasBoolAttribute(attr.first))
        << attributeErrorMessage(std::string(ui::ToString(attr.first)));
  }

  for (const auto& attr : new_data.float_attributes) {
    DCHECK(!existing_data.HasFloatAttribute(attr.first))
        << attributeErrorMessage(std::string(ui::ToString(attr.first)));
  }

  for (const auto& attr : new_data.intlist_attributes) {
    DCHECK(!existing_data.HasIntListAttribute(attr.first))
        << attributeErrorMessage(std::string(ui::ToString(attr.first)));
  }

  for (const auto& attr : new_data.stringlist_attributes) {
    DCHECK(!existing_data.HasStringListAttribute(attr.first))
        << attributeErrorMessage(std::string(ui::ToString(attr.first)));
  }

  auto bitfieldErrorMessage = [](std::string bitfield_name) -> std::string {
    return "The accessible " + bitfield_name +
           " should be set directly in the accessibility cache through the "
           "ViewAccessibility setters, not through this lazy loading "
           "mechanism. The purpose of the lazy loading mechanism is to avoid "
           "expensive memory allocations and calculations until the "
           "accessibility tree is actually needed. However, the state is "
           "stored efficiently in a bitfield always initialized to zero, so "
           "there's not performance improvement to be gained by lazy loading "
           "it.";
  };

  DCHECK(new_data.state == 0U) << bitfieldErrorMessage("state");
  DCHECK(new_data.actions == 0U) << bitfieldErrorMessage("action");
  DCHECK(new_data.relative_bounds.bounds.IsEmpty())
      << "The `relative_bounds` should not be set in the lazy loading "
         "function. Instead, use `ViewAccessibility::SetBounds.`";
}

}  // namespace views
