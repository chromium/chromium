// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/fuchsia/browser_accessibility_fuchsia.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/fuchsia/browser_accessibility_manager_fuchsia.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_registry.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

using AXRole = ax::mojom::Role;
using FuchsiaRole = fuchsia_accessibility_semantics::Role;

BrowserAccessibilityFuchsia::BrowserAccessibilityFuchsia(
    BrowserAccessibilityManager* manager,
    AXNode* node)
    : BrowserAccessibility(manager, node) {
  platform_node_ =
      static_cast<AXPlatformNodeFuchsia*>(AXPlatformNode::Create(this));
}

AccessibilityBridgeFuchsia*
BrowserAccessibilityFuchsia::GetAccessibilityBridge() const {
  BrowserAccessibilityManagerFuchsia* manager_fuchsia =
      static_cast<BrowserAccessibilityManagerFuchsia*>(manager());
  DCHECK(manager_fuchsia);

  return manager_fuchsia->GetAccessibilityBridge();
}

// static
std::unique_ptr<BrowserAccessibility> BrowserAccessibility::Create(
    BrowserAccessibilityManager* manager,
    AXNode* node) {
  return std::make_unique<BrowserAccessibilityFuchsia>(manager, node);
}

BrowserAccessibilityFuchsia::~BrowserAccessibilityFuchsia() {
  DeleteNode();
  platform_node_->Destroy();
}

uint32_t BrowserAccessibilityFuchsia::GetFuchsiaNodeID() const {
  return static_cast<uint32_t>(GetUniqueId());
}

fuchsia_accessibility_semantics::Node
BrowserAccessibilityFuchsia::ToFuchsiaNodeData() const {
  return {{
      .node_id = GetFuchsiaNodeID(),
      .role = GetFuchsiaRole(),
      .states = GetFuchsiaStates(),
      .attributes = GetFuchsiaAttributes(),
      .actions = GetFuchsiaActions(),
      .child_ids = GetFuchsiaChildIDs(),
      .location = GetFuchsiaLocation(),
      .container_id = GetOffsetContainerOrRootNodeID(),
      .node_to_container_transform = GetFuchsiaTransform(),
  }};
}

void BrowserAccessibilityFuchsia::OnDataChanged() {
  BrowserAccessibility::OnDataChanged();

  // Declare this node as the fuchsia tree root if it's the root of the main
  // frame's tree.
  if (manager()->IsRootFrameManager() &&
      manager()->GetBrowserAccessibilityRoot() == this) {
    AccessibilityBridgeFuchsia* accessibility_bridge = GetAccessibilityBridge();
    if (accessibility_bridge)
      accessibility_bridge->SetRootID(GetUniqueId());
  }

  UpdateNode();
}

void BrowserAccessibilityFuchsia::OnLocationChanged() {
  UpdateNode();
}

AXPlatformNode* BrowserAccessibilityFuchsia::GetAXPlatformNode() const {
  return platform_node_;
}

BrowserAccessibilityFuchsia* ToBrowserAccessibilityFuchsia(
    BrowserAccessibility* obj) {
  return static_cast<BrowserAccessibilityFuchsia*>(obj);
}

std::vector<uint32_t> BrowserAccessibilityFuchsia::GetFuchsiaChildIDs() const {
  std::vector<uint32_t> child_ids;

  // TODO(abrusher): Switch back to using platform children.
  for (const auto* child : AllChildren()) {
    const BrowserAccessibilityFuchsia* fuchsia_child =
        static_cast<const BrowserAccessibilityFuchsia*>(child);
    DCHECK(fuchsia_child);

    child_ids.push_back(fuchsia_child->GetFuchsiaNodeID());
  }

  return child_ids;
}

bool BrowserAccessibilityFuchsia::IsFuchsiaDefaultAction() const {
  // Nodes given the Default action map to Fuchsia's Default action.
  if (HasAction(ax::mojom::Action::kDoDefault)) {
    return true;
  }
  // Action verbs other than None and ClickAncestor also map to the Fuchsia's
  // Default action. ClickAncestor should be excluded, as the node itself is not
  // clickable (only an ancestor in the tree).
  auto verb = GetData().GetDefaultActionVerb();
  return verb != ax::mojom::DefaultActionVerb::kNone &&
         verb != ax::mojom::DefaultActionVerb::kClickAncestor;
}

std::vector<fuchsia_accessibility_semantics::Action>
BrowserAccessibilityFuchsia::GetFuchsiaActions() const {
  std::vector<fuchsia_accessibility_semantics::Action> actions;

  if (IsFuchsiaDefaultAction()) {
    actions.push_back(fuchsia_accessibility_semantics::Action::kDefault);
  }

  if (HasAction(ax::mojom::Action::kFocus))
    actions.push_back(fuchsia_accessibility_semantics::Action::kSetFocus);

  if (HasAction(ax::mojom::Action::kSetValue))
    actions.push_back(fuchsia_accessibility_semantics::Action::kSetValue);

  if (HasAction(ax::mojom::Action::kScrollToMakeVisible)) {
    actions.push_back(fuchsia_accessibility_semantics::Action::kShowOnScreen);
  }

  return actions;
}

fuchsia_accessibility_semantics::Role
BrowserAccessibilityFuchsia::GetFuchsiaRole() const {
  auto role = GetRole();

  switch (role) {
    case AXRole::kButton:
      return FuchsiaRole::kButton;
    case AXRole::kCell:
      return FuchsiaRole::kCell;
    case AXRole::kCheckBox:
      return FuchsiaRole::kCheckBox;
    case AXRole::kColumnHeader:
      return FuchsiaRole::kColumnHeader;
    case AXRole::kGrid:
      return FuchsiaRole::kGrid;
    case AXRole::kGridCell:
      return FuchsiaRole::kCell;
    case AXRole::kHeader:
      return FuchsiaRole::kHeader;
    case AXRole::kImage:
      return FuchsiaRole::kImage;
    case AXRole::kLink:
      return FuchsiaRole::kLink;
    case AXRole::kList:
      return FuchsiaRole::kList;
    case AXRole::kListItem:
      return FuchsiaRole::kListElement;
    case AXRole::kListMarker:
      return FuchsiaRole::kListElementMarker;
    case AXRole::kParagraph:
      return FuchsiaRole::kParagraph;
    case AXRole::kRadioButton:
      return FuchsiaRole::kRadioButton;
    case AXRole::kRowGroup:
      return FuchsiaRole::kRowGroup;
    case AXRole::kSearchBox:
      return FuchsiaRole::kSearchBox;
    case AXRole::kSlider:
      return FuchsiaRole::kSlider;
    case AXRole::kStaticText:
      return FuchsiaRole::kStaticText;
    case AXRole::kTable:
      return FuchsiaRole::kTable;
    case AXRole::kRow:
      return FuchsiaRole::kTableRow;
    case AXRole::kTextField:
      return FuchsiaRole::kTextField;
    case AXRole::kTextFieldWithComboBox:
      return FuchsiaRole::kTextFieldWithComboBox;
    default:
      return FuchsiaRole::kUnknown;
  }
}

fuchsia_accessibility_semantics::States
BrowserAccessibilityFuchsia::GetFuchsiaStates() const {
  fuchsia_accessibility_semantics::States states;

  // Convert checked state.
  if (HasIntAttribute(ax::mojom::IntAttribute::kCheckedState)) {
    ax::mojom::CheckedState ax_state = GetData().GetCheckedState();
    switch (ax_state) {
      case ax::mojom::CheckedState::kNone:
        states.checked_state(
            fuchsia_accessibility_semantics::CheckedState::kNone);
        break;
      case ax::mojom::CheckedState::kTrue:
        states.checked_state(
            fuchsia_accessibility_semantics::CheckedState::kChecked);
        break;
      case ax::mojom::CheckedState::kFalse:
        states.checked_state(
            fuchsia_accessibility_semantics::CheckedState::kUnchecked);
        break;
      case ax::mojom::CheckedState::kMixed:
        states.checked_state(
            fuchsia_accessibility_semantics::CheckedState::kMixed);
        break;
    }
  }

  // Convert selected state.
  // Indicates whether a node has been selected.
  if (GetData().IsSelectable() &&
      HasBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
    states.selected(GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  }

  // Indicates if the node is hidden.
  states.hidden(IsInvisibleOrIgnored());

  // The user entered value of the node, if applicable.
  if (HasStringAttribute(ax::mojom::StringAttribute::kValue)) {
    const std::string& value =
        GetStringAttribute(ax::mojom::StringAttribute::kValue);
    states.value(
        value.substr(0, fuchsia_accessibility_semantics::kMaxLabelSize));
  }

  // The value a range element currently has.
  if (HasFloatAttribute(ax::mojom::FloatAttribute::kValueForRange)) {
    states.range_value(
        GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange));
  }

  // The scroll offsets, if the element is a scrollable container.
  const float x_scroll_offset =
      GetIntAttribute(ax::mojom::IntAttribute::kScrollX);
  const float y_scroll_offset =
      GetIntAttribute(ax::mojom::IntAttribute::kScrollY);
  if (x_scroll_offset || y_scroll_offset)
    states.viewport_offset({{x_scroll_offset, y_scroll_offset}});

  if (IsFocusable())
    states.focusable(true);

  states.has_input_focus(IsFocused());

  return states;
}

fuchsia_accessibility_semantics::Attributes
BrowserAccessibilityFuchsia::GetFuchsiaAttributes() const {
  fuchsia_accessibility_semantics::Attributes attributes;
  if (HasStringAttribute(ax::mojom::StringAttribute::kName)) {
    const std::string& name =
        GetStringAttribute(ax::mojom::StringAttribute::kName);
    attributes.label(
        name.substr(0, fuchsia_accessibility_semantics::kMaxLabelSize));
  }

  if (HasStringAttribute(ax::mojom::StringAttribute::kDescription)) {
    const std::string& description =
        GetStringAttribute(ax::mojom::StringAttribute::kDescription);
    attributes.secondary_label(
        description.substr(0, fuchsia_accessibility_semantics::kMaxLabelSize));
  }

  if (GetData().IsRangeValueSupported()) {
    fuchsia_accessibility_semantics::RangeAttributes range_attributes;
    if (HasFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange)) {
      range_attributes.min_value(
          GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange));
    }
    if (HasFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange)) {
      range_attributes.max_value(
          GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange));
    }
    if (HasFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange)) {
      range_attributes.step_delta(
          GetFloatAttribute(ax::mojom::FloatAttribute::kStepValueForRange));
    }
    attributes.range(std::move(range_attributes));
  }

  if (IsTable()) {
    fuchsia_accessibility_semantics::TableAttributes table_attributes;
    auto col_count = GetTableColCount();
    if (col_count)
      table_attributes.number_of_columns(*col_count);

    auto row_count = GetTableRowCount();
    if (row_count)
      table_attributes.number_of_rows(*row_count);

    if (!table_attributes.IsEmpty())
      attributes.table_attributes(std::move(table_attributes));
  }

  if (IsTableRow()) {
    fuchsia_accessibility_semantics::TableRowAttributes table_row_attributes;
    auto row_index = GetTableRowRowIndex();
    if (row_index) {
      table_row_attributes.row_index(*row_index);
      attributes.table_row_attributes(std::move(table_row_attributes));
    }
  }

  if (IsTableCellOrHeader()) {
    fuchsia_accessibility_semantics::TableCellAttributes table_cell_attributes;

    auto col_index = GetTableCellColIndex();
    if (col_index)
      table_cell_attributes.column_index(*col_index);

    auto row_index = GetTableCellRowIndex();
    if (row_index)
      table_cell_attributes.row_index(*row_index);

    auto col_span = GetTableCellColSpan();
    if (col_span)
      table_cell_attributes.column_span(*col_span);

    auto row_span = GetTableCellRowSpan();
    if (row_span)
      table_cell_attributes.row_span(*row_span);

    if (!table_cell_attributes.IsEmpty())
      attributes.table_cell_attributes(std::move(table_cell_attributes));
  }

  if (IsList()) {
    std::optional<int> size = GetSetSize();
    if (size) {
      fuchsia_accessibility_semantics::SetAttributes list_attributes;
      list_attributes.size(*size);
      attributes.list_attributes(std::move(list_attributes));
    }
  }

  if (IsListElement()) {
    std::optional<int> index = GetPosInSet();
    if (index) {
      fuchsia_accessibility_semantics::SetAttributes list_element_attributes;
      list_element_attributes.index(*index);
      attributes.list_element_attributes(std::move(list_element_attributes));
    }
  }

  return attributes;
}

fuchsia_ui_gfx::BoundingBox BrowserAccessibilityFuchsia::GetFuchsiaLocation()
    const {
  const gfx::RectF& bounds = GetLocation();

  return {{
      .min = {{
          .x = bounds.x(),
          .y = bounds.y(),
          .z = 0.0f,
      }},
      .max = {{
          .x = bounds.right(),
          .y = bounds.bottom(),
          .z = 0.0f,
      }},
  }};
}

fuchsia_ui_gfx::Mat4 BrowserAccessibilityFuchsia::GetFuchsiaTransform() const {
  // Get AXNode's explicit transform.
  gfx::Transform transform;
  if (GetData().relative_bounds.transform)
    transform = *GetData().relative_bounds.transform;

  // Convert to fuchsia's transform type.
  std::array<float, 16> mat = {};
  transform.GetColMajorF(mat.data());
  return {{.matrix = mat}};
}

uint32_t BrowserAccessibilityFuchsia::GetOffsetContainerOrRootNodeID() const {
  int offset_container_id = GetData().relative_bounds.offset_container_id;

  BrowserAccessibility* offset_container =
      offset_container_id == -1 ? manager()->GetBrowserAccessibilityRoot()
                                : manager()->GetFromID(offset_container_id);

  BrowserAccessibilityFuchsia* fuchsia_container =
      ToBrowserAccessibilityFuchsia(offset_container);

  // TODO(crbug.com/40837684): Remove this check once we understand why
  // we're getting non-existent offset container IDs from blink.
  if (!fuchsia_container) {
    ZX_LOG(ERROR, ZX_OK) << "Node " << GetId()
                         << " references non-existent offset container ID "
                         << offset_container_id;
    return 0;
  }

  return fuchsia_container->GetFuchsiaNodeID();
}

void BrowserAccessibilityFuchsia::UpdateNode() {
  if (!GetAccessibilityBridge())
    return;

  GetAccessibilityBridge()->UpdateNode(ToFuchsiaNodeData());
}

void BrowserAccessibilityFuchsia::DeleteNode() {
  if (!GetAccessibilityBridge())
    return;

  GetAccessibilityBridge()->DeleteNode(GetFuchsiaNodeID());
}

bool BrowserAccessibilityFuchsia::IsList() const {
  return GetRole() == AXRole::kList;
}

bool BrowserAccessibilityFuchsia::IsListElement() const {
  return GetRole() == AXRole::kListItem;
}

bool BrowserAccessibilityFuchsia::AccessibilityPerformAction(
    const AXActionData& action_data) {
  if (action_data.action == ax::mojom::Action::kHitTest) {
    BrowserAccessibilityManager* root_manager =
        manager()->GetManagerForRootFrame();
    DCHECK(root_manager);

    AccessibilityBridgeFuchsia* accessibility_bridge = GetAccessibilityBridge();
    if (!accessibility_bridge)
      return false;

    root_manager->HitTest(action_data.target_point, action_data.request_id);
    return true;
  }

  return BrowserAccessibility::AccessibilityPerformAction(action_data);
}

}  // namespace ui
