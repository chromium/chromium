// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_accessibility_utils.h"

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
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
    destination.AddStringAttribute(attr.first, attr.second);
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

  // TODO(javiercon): Add checking for all the states, and add DCHECK for them
  // as well. Do the same thing for the Restrictions.
  if (source.HasState(ax::mojom::State::kIgnored)) {
    destination.AddState(ax::mojom::State::kIgnored);
  }
}

#if DCHECK_IS_ON()

std::unordered_set<ax::mojom::IntAttribute>&
ViewsAXCompletedAttributes::int_attr_set() {
  static std::unordered_set<ax::mojom::IntAttribute> set;
  return set;
}

std::unordered_set<ax::mojom::StringAttribute>&
ViewsAXCompletedAttributes::string_attr_set() {
  static std::unordered_set<ax::mojom::StringAttribute> set;
  return set;
}

std::unordered_set<ax::mojom::BoolAttribute>&
ViewsAXCompletedAttributes::bool_attr_set() {
  static std::unordered_set<ax::mojom::BoolAttribute> set;
  return set;
}

std::unordered_set<ax::mojom::IntListAttribute>&
ViewsAXCompletedAttributes::int_list_attr_set() {
  static std::unordered_set<ax::mojom::IntListAttribute> set;
  return set;
}

std::unordered_set<ax::mojom::StringListAttribute>&
ViewsAXCompletedAttributes::string_list_attr_set() {
  static std::unordered_set<ax::mojom::StringListAttribute> set;
  return set;
}

void ViewsAXCompletedAttributes::Validate(
    const ui::AXNodeData& data_to_validate) {
  for (const auto& attr : data_to_validate.int_attributes) {
    DCHECK(!base::Contains(int_attr_set(), attr.first))
        << " Attribute " << attr.first
        << " has been migrated to use the new AXNodeData pipeline. Please use "
           "the setters/getters in ViewAccessibility to set the attribute."
           "See the comment in ViewAccessibility::GetAccessibleNodeData for "
           "more info.";
  }

  for (const auto& attr : data_to_validate.string_attributes) {
    DCHECK(!base::Contains(string_attr_set(), attr.first))
        << " Attribute " << attr.first
        << " has been migrated to use the new AXNodeData pipeline. Please use "
           "the setters/getters in ViewAccessibility to set the attribute."
           "See the comment in ViewAccessibility::GetAccessibleNodeData for "
           "more info.";
  }

  for (const auto& attr : data_to_validate.bool_attributes) {
    DCHECK(!base::Contains(bool_attr_set(), attr.first))
        << " Attribute " << attr.first
        << " has been migrated to use the new AXNodeData pipeline. Please use "
           "the setters/getters in ViewAccessibility to set the attribute."
           "See the comment in ViewAccessibility::GetAccessibleNodeData for "
           "more info.";
  }

  for (const auto& attr : data_to_validate.intlist_attributes) {
    DCHECK(!base::Contains(int_list_attr_set(), attr.first))
        << " Attribute " << attr.first
        << " has been migrated to use the new AXNodeData pipeline. Please use "
           "the setters/getters in ViewAccessibility to set the attribute."
           "See the comment in ViewAccessibility::GetAccessibleNodeData for "
           "more info.";
  }

  for (const auto& attr : data_to_validate.stringlist_attributes) {
    DCHECK(!base::Contains(string_list_attr_set(), attr.first))
        << " Attribute " << attr.first
        << " has been migrated to use the new AXNodeData pipeline. Please use "
           "the setters/getters in ViewAccessibility to set the attribute."
           "See the comment in ViewAccessibility::GetAccessibleNodeData for "
           "more info.";
  }
}
#endif  // DCHECK_IS_ON()

}  // namespace views
