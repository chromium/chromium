// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_UTILS_H_
#define UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_UTILS_H_

#include <unordered_set>
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

class VIEWS_EXPORT ViewAccessibilityUtils {
 public:
  // Returns a focused child widget if the view has a child that should be
  // treated as a special case. For example, if a tab modal dialog is visible
  // and focused, this will return the dialog when called on the BrowserView.
  // This helper function is used to treat such widgets as separate windows for
  // accessibility. Returns nullptr if no such widget is present.
  static Widget* GetFocusedChildWidgetForAccessibility(const View* view);

  // Used by GetFocusedChildWidgetForAccessibility to determine if a Widget
  // should be handled separately.
  static bool IsFocusedChildWidget(Widget* widget, const View* focused_view);

  static void Merge(const ui::AXNodeData& source, ui::AXNodeData& destination);
};

#if DCHECK_IS_ON()
// This is a class intended to keep track of the attributes that have already
// been migrated from the old system of computing AXNodeData for Views (pull),
// to the new system (push). This will help ensure that new Views don't use the
// old system for attributes that have already been migrated, since the
// migration will take some time. Once the migration is complete, this class
// will be removed.
// TODO(accessibility): Remove once migration is complete.
class VIEWS_EXPORT ViewsAXCompletedAttributes {
 public:
  ViewsAXCompletedAttributes() = delete;
  ~ViewsAXCompletedAttributes() = delete;

  // Makes sure that there are no attributes in the sets that have already been
  // set in `data_to_validate`.
  static void Validate(const ui::AXNodeData& data_to_validate);

 private:
  static std::unordered_set<ax::mojom::IntAttribute>& int_attr_set();
  static std::unordered_set<ax::mojom::StringAttribute>& string_attr_set();
  static std::unordered_set<ax::mojom::BoolAttribute>& bool_attr_set();
  static std::unordered_set<ax::mojom::IntListAttribute>& int_list_attr_set();
  static std::unordered_set<ax::mojom::StringListAttribute>&
  string_list_attr_set();
};
#endif  // DCHECK_IS_ON()

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_UTILS_H_
