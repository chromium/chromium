// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_delegate.h"

#include "base/containers/fixed_flat_set.h"
#include "base/notreached.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"

namespace ui {

AXPlatformNodeDelegate::AXPlatformNodeDelegate() : node_(nullptr) {}

AXPlatformNodeDelegate::AXPlatformNodeDelegate(ui::AXNode* node) : node_(node) {
  DCHECK(node);
  DCHECK(node->IsDataValid());
}

void AXPlatformNodeDelegate::SetNode(AXNode& node) {
  DCHECK(node.IsDataValid());
  node_ = &node;
}

ui::AXNodeID AXPlatformNodeDelegate::GetId() const {
  if (node_)
    return node_->id();
  return kInvalidAXNodeID;
}

AXTreeManager* AXPlatformNodeDelegate::GetTreeManager() const {
  return AXTreeManager::FromID(GetTreeData().tree_id);
}

const AXTreeData& AXPlatformNodeDelegate::GetTreeData() const {
  if (node_) {
    DCHECK(node_->tree())
        << "All nodes should be owned by an accessibility tree.\n"
        << *node_;
    return node_->tree()->data();
  }

  static base::NoDestructor<AXTreeData> empty_data;
  return *empty_data;
}

ax::mojom::Role AXPlatformNodeDelegate::GetRole() const {
  // Getting the role is generally the result of an accessibility API call, so
  // we should reset the auto-disable accessibility code.
  NotifyAccessibilityApiUsage();
  if (node_)
    return node_->GetRole();
  return GetData().role;
}

bool AXPlatformNodeDelegate::HasBoolAttribute(
    ax::mojom::BoolAttribute attribute) const {
  if (node_)
    return node_->HasBoolAttribute(attribute);
  return GetData().HasBoolAttribute(attribute);
}

bool AXPlatformNodeDelegate::GetBoolAttribute(
    ax::mojom::BoolAttribute attribute) const {
  if (node_)
    return node_->GetBoolAttribute(attribute);
  return GetData().GetBoolAttribute(attribute);
}

bool AXPlatformNodeDelegate::GetBoolAttribute(
    ax::mojom::BoolAttribute attribute,
    bool* value) const {
  if (node_)
    return node_->GetBoolAttribute(attribute, value);
  return GetData().GetBoolAttribute(attribute, value);
}

bool AXPlatformNodeDelegate::HasFloatAttribute(
    ax::mojom::FloatAttribute attribute) const {
  if (node_)
    return node_->HasFloatAttribute(attribute);
  return GetData().HasFloatAttribute(attribute);
}

float AXPlatformNodeDelegate::GetFloatAttribute(
    ax::mojom::FloatAttribute attribute) const {
  if (node_)
    return node_->GetFloatAttribute(attribute);
  return GetData().GetFloatAttribute(attribute);
}

bool AXPlatformNodeDelegate::GetFloatAttribute(
    ax::mojom::FloatAttribute attribute,
    float* value) const {
  if (node_)
    return node_->GetFloatAttribute(attribute, value);
  return GetData().GetFloatAttribute(attribute, value);
}

const std::vector<std::pair<ax::mojom::IntAttribute, int32_t>>&
AXPlatformNodeDelegate::GetIntAttributes() const {
  if (node_)
    return node_->GetIntAttributes();
  return GetData().int_attributes;
}

bool AXPlatformNodeDelegate::HasIntAttribute(
    ax::mojom::IntAttribute attribute) const {
  if (node_)
    return node_->HasIntAttribute(attribute);
  return GetData().HasIntAttribute(attribute);
}

int AXPlatformNodeDelegate::GetIntAttribute(
    ax::mojom::IntAttribute attribute) const {
  if (node_)
    return node_->GetIntAttribute(attribute);
  return GetData().GetIntAttribute(attribute);
}

bool AXPlatformNodeDelegate::GetIntAttribute(ax::mojom::IntAttribute attribute,
                                             int* value) const {
  if (node_)
    return node_->GetIntAttribute(attribute, value);
  return GetData().GetIntAttribute(attribute, value);
}

const std::vector<std::pair<ax::mojom::StringAttribute, std::string>>&
AXPlatformNodeDelegate::GetStringAttributes() const {
  if (node_)
    return node_->GetStringAttributes();
  return GetData().string_attributes;
}

bool AXPlatformNodeDelegate::HasStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  if (node_)
    return node_->HasStringAttribute(attribute);
  return GetData().HasStringAttribute(attribute);
}

const std::string& AXPlatformNodeDelegate::GetStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  if (node_)
    return node_->GetStringAttribute(attribute);
  return GetData().GetStringAttribute(attribute);
}

bool AXPlatformNodeDelegate::GetStringAttribute(
    ax::mojom::StringAttribute attribute,
    std::string* value) const {
  if (node_)
    return node_->GetStringAttribute(attribute, value);
  return GetData().GetStringAttribute(attribute, value);
}

std::u16string AXPlatformNodeDelegate::GetString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  if (node_)
    return node_->GetString16Attribute(attribute);
  return GetData().GetString16Attribute(attribute);
}

bool AXPlatformNodeDelegate::GetString16Attribute(
    ax::mojom::StringAttribute attribute,
    std::u16string* value) const {
  if (node_)
    return node_->GetString16Attribute(attribute, value);
  return GetData().GetString16Attribute(attribute, value);
}

const std::string& AXPlatformNodeDelegate::GetInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  if (node_)
    return node_->GetInheritedStringAttribute(attribute);

  NOTIMPLEMENTED();
  return GetData().GetStringAttribute(attribute);
}

std::u16string AXPlatformNodeDelegate::GetInheritedString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  if (node_)
    return node_->GetInheritedString16Attribute(attribute);

  NOTIMPLEMENTED();
  return GetData().GetString16Attribute(attribute);
}

const std::vector<std::pair<ax::mojom::IntListAttribute, std::vector<int32_t>>>&
AXPlatformNodeDelegate::GetIntListAttributes() const {
  if (node_)
    return node_->GetIntListAttributes();
  return GetData().intlist_attributes;
}

bool AXPlatformNodeDelegate::HasIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  if (node_)
    return node_->HasIntListAttribute(attribute);
  return GetData().HasIntListAttribute(attribute);
}

const std::vector<int32_t>& AXPlatformNodeDelegate::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  if (node_)
    return node_->GetIntListAttribute(attribute);
  return GetData().GetIntListAttribute(attribute);
}

bool AXPlatformNodeDelegate::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute,
    std::vector<int32_t>* value) const {
  if (node_)
    return node_->GetIntListAttribute(attribute, value);
  return GetData().GetIntListAttribute(attribute, value);
}

bool AXPlatformNodeDelegate::HasStringListAttribute(
    ax::mojom::StringListAttribute attribute) const {
  if (node_)
    return node_->HasStringListAttribute(attribute);
  return GetData().HasStringListAttribute(attribute);
}

const std::vector<std::string>& AXPlatformNodeDelegate::GetStringListAttribute(
    ax::mojom::StringListAttribute attribute) const {
  if (node_)
    return node_->GetStringListAttribute(attribute);
  return GetData().GetStringListAttribute(attribute);
}

bool AXPlatformNodeDelegate::GetStringListAttribute(
    ax::mojom::StringListAttribute attribute,
    std::vector<std::string>* value) const {
  if (node_)
    return node_->GetStringListAttribute(attribute, value);
  return GetData().GetStringListAttribute(attribute, value);
}

bool AXPlatformNodeDelegate::HasHtmlAttribute(const char* attribute) const {
  if (node_)
    return node_->HasHtmlAttribute(attribute);
  return GetData().HasHtmlAttribute(attribute);
}

const base::StringPairs& AXPlatformNodeDelegate::GetHtmlAttributes() const {
  if (node_)
    return node_->GetHtmlAttributes();
  return GetData().html_attributes;
}

bool AXPlatformNodeDelegate::GetHtmlAttribute(const char* attribute,
                                              std::string* value) const {
  if (node_)
    return node_->GetHtmlAttribute(attribute, value);
  return GetData().GetHtmlAttribute(attribute, value);
}

bool AXPlatformNodeDelegate::GetHtmlAttribute(const char* attribute,
                                              std::u16string* value) const {
  if (node_)
    return node_->GetHtmlAttribute(attribute, value);
  return GetData().GetHtmlAttribute(attribute, value);
}

AXTextAttributes AXPlatformNodeDelegate::GetTextAttributes() const {
  if (node_)
    return node_->GetTextAttributes();
  return GetData().GetTextAttributes();
}

bool AXPlatformNodeDelegate::HasState(ax::mojom::State state) const {
  if (node_)
    return node_->HasState(state);
  return GetData().HasState(state);
}

ax::mojom::State AXPlatformNodeDelegate::GetState() const {
  if (node_)
    return node_->GetState();
  return static_cast<ax::mojom::State>(GetData().state);
}

bool AXPlatformNodeDelegate::HasAction(ax::mojom::Action action) const {
  if (node_)
    return node_->HasAction(action);
  return GetData().HasAction(action);
}

gfx::Rect AXPlatformNodeDelegate::GetClippedScreenBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kScreenDIPs,
                       AXClippingBehavior::kClipped, offscreen_result);
}

gfx::Rect AXPlatformNodeDelegate::GetUnclippedScreenBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kScreenDIPs,
                       AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect AXPlatformNodeDelegate::GetClippedRootFrameBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kRootFrame,
                       AXClippingBehavior::kClipped, offscreen_result);
}

gfx::Rect AXPlatformNodeDelegate::GetUnclippedRootFrameBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kRootFrame,
                       AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::Rect AXPlatformNodeDelegate::GetClippedFrameBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kFrame, AXClippingBehavior::kClipped,
                       offscreen_result);
}

gfx::Rect AXPlatformNodeDelegate::GetUnclippedFrameBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kFrame,
                       AXClippingBehavior::kUnclipped, offscreen_result);
}

bool AXPlatformNodeDelegate::HasDefaultActionVerb() const {
  return GetData().GetDefaultActionVerb() !=
         ax::mojom::DefaultActionVerb::kNone;
}

std::vector<ax::mojom::Action> AXPlatformNodeDelegate::GetSupportedActions()
    const {
  static constexpr auto kActionsThatCanBeExposed =
      base::MakeFixedFlatSet<ax::mojom::Action>(
          {ax::mojom::Action::kDecrement, ax::mojom::Action::kIncrement,
           ax::mojom::Action::kScrollUp, ax::mojom::Action::kScrollDown,
           ax::mojom::Action::kScrollLeft, ax::mojom::Action::kScrollRight,
           ax::mojom::Action::kScrollForward,
           ax::mojom::Action::kScrollBackward});
  std::vector<ax::mojom::Action> supported_actions;

  // The default action must be listed at index 0.
  // TODO(crbug.com/1370076): Find out why some nodes do not expose a
  // default action (HasDefaultActionVerb() is false).
  supported_actions.push_back(ax::mojom::Action::kDoDefault);

  // Users expect to be able to bring a context menu on any object via e.g.
  // right click, so we make the context menu action available to any object
  // unconditionally.
  supported_actions.push_back(ax::mojom::Action::kShowContextMenu);

  for (const auto& item : kActionsThatCanBeExposed) {
    if (HasAction(item))
      supported_actions.push_back(item);
  }

  return supported_actions;
}

bool AXPlatformNodeDelegate::HasTextStyle(
    ax::mojom::TextStyle text_style) const {
  if (node_)
    return node_->HasTextStyle(text_style);
  return GetData().HasTextStyle(text_style);
}

ax::mojom::NameFrom AXPlatformNodeDelegate::GetNameFrom() const {
  if (node_)
    return node_->GetNameFrom();
  return GetData().GetNameFrom();
}

ax::mojom::DescriptionFrom AXPlatformNodeDelegate::GetDescriptionFrom() const {
  if (node_)
    return node_->GetDescriptionFrom();
  return GetData().GetDescriptionFrom();
}

const AXSelection AXPlatformNodeDelegate::GetUnignoredSelection() const {
  if (node_)
    return node_->GetUnignoredSelection();

  NOTIMPLEMENTED();
  return AXSelection();
}

bool AXPlatformNodeDelegate::IsLeaf() const {
  if (node_)
    return node_->IsLeaf();
  return !GetChildCount();
}

bool AXPlatformNodeDelegate::IsInvisibleOrIgnored() const {
  if (node_)
    return node_->IsInvisibleOrIgnored();
  return IsIgnored() || GetData().IsInvisible();
}

const std::string& AXPlatformNodeDelegate::GetName() const {
  if (node_)
    return node()->GetNameUTF8();
  return GetStringAttribute(ax::mojom::StringAttribute::kName);
}

bool AXPlatformNodeDelegate::IsTable() const {
  if (node_)
    return node_->IsTable();
  return ui::IsTableLike(GetRole());
}

absl::optional<int> AXPlatformNodeDelegate::GetTableRowCount() const {
  if (node_)
    return node_->GetTableRowCount();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableRowCount);
}

absl::optional<int> AXPlatformNodeDelegate::GetTableColCount() const {
  if (node_)
    return node_->GetTableColCount();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableColumnCount);
}

absl::optional<int> AXPlatformNodeDelegate::GetTableCellCount() const {
  if (node_)
    return node_->GetTableCellCount();
  return absl::nullopt;
}

absl::optional<bool> AXPlatformNodeDelegate::GetTableHasColumnOrRowHeaderNode()
    const {
  if (node_)
    return node_->GetTableHasColumnOrRowHeaderNode();
  return absl::nullopt;
}

std::vector<int32_t> AXPlatformNodeDelegate::GetColHeaderNodeIds() const {
  if (node_)
    return node_->GetTableColHeaderNodeIds();
  return {};
}

std::vector<int32_t> AXPlatformNodeDelegate::GetColHeaderNodeIds(
    int col_index) const {
  if (node_)
    return node_->GetTableColHeaderNodeIds(col_index);
  return {};
}

std::vector<int32_t> AXPlatformNodeDelegate::GetRowHeaderNodeIds() const {
  if (node_)
    return node_->GetTableCellRowHeaderNodeIds();
  return {};
}

std::vector<int32_t> AXPlatformNodeDelegate::GetRowHeaderNodeIds(
    int row_index) const {
  if (node_)
    return node_->GetTableRowHeaderNodeIds(row_index);
  return {};
}

bool AXPlatformNodeDelegate::IsTableRow() const {
  if (node_)
    return node_->IsTableRow();
  return ui::IsTableRow(GetRole());
}

absl::optional<int> AXPlatformNodeDelegate::GetTableRowRowIndex() const {
  if (node_)
    return node_->GetTableRowRowIndex();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableRowIndex);
}

bool AXPlatformNodeDelegate::IsTableCellOrHeader() const {
  if (node_)
    return node_->IsTableCellOrHeader();
  return ui::IsCellOrTableHeader(GetRole());
}

absl::optional<int> AXPlatformNodeDelegate::GetTableCellIndex() const {
  if (node_)
    return node_->GetTableCellIndex();
  return absl::nullopt;
}

absl::optional<int> AXPlatformNodeDelegate::GetTableCellColIndex() const {
  if (node_)
    return node_->GetTableCellColIndex();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex);
}

absl::optional<int> AXPlatformNodeDelegate::GetTableCellRowIndex() const {
  if (node_)
    return node_->GetTableCellRowIndex();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex);
}

absl::optional<int> AXPlatformNodeDelegate::GetTableCellColSpan() const {
  if (node_)
    return node_->GetTableCellColSpan();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan);
}

absl::optional<int> AXPlatformNodeDelegate::GetTableCellRowSpan() const {
  if (node_)
    return node_->GetTableCellRowSpan();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan);
}

absl::optional<int> AXPlatformNodeDelegate::GetTableCellAriaColIndex() const {
  if (node_)
    return node_->GetTableCellAriaColIndex();
  if (HasIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnIndex))
    return GetIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnIndex);
  return absl::nullopt;
}

absl::optional<int> AXPlatformNodeDelegate::GetTableCellAriaRowIndex() const {
  if (node_)
    return node_->GetTableCellAriaRowIndex();
  if (HasIntAttribute(ax::mojom::IntAttribute::kAriaCellRowIndex))
    return GetIntAttribute(ax::mojom::IntAttribute::kAriaCellRowIndex);
  return absl::nullopt;
}

absl::optional<int32_t> AXPlatformNodeDelegate::GetCellId(int row_index,
                                                          int col_index) const {
  if (node_) {
    AXNode* cell = node()->GetTableCellFromCoords(row_index, col_index);
    if (!cell)
      return absl::nullopt;
    return cell->id();
  }
  return absl::nullopt;
}

absl::optional<int32_t> AXPlatformNodeDelegate::CellIndexToId(
    int cell_index) const {
  if (node_) {
    ui::AXNode* cell = node()->GetTableCellFromIndex(cell_index);
    if (!cell)
      return absl::nullopt;
    return cell->id();
  }
  return absl::nullopt;
}

bool AXPlatformNodeDelegate::IsCellOrHeaderOfAriaGrid() const {
  if (node_)
    return node_->IsCellOrHeaderOfAriaGrid();
  return false;
}

bool AXPlatformNodeDelegate::IsOrderedSetItem() const {
  if (node_)
    return node_->IsOrderedSetItem();
  return false;
}

bool AXPlatformNodeDelegate::IsOrderedSet() const {
  if (node_)
    return node_->IsOrderedSet();
  return false;
}

SkColor AXPlatformNodeDelegate::GetColor() const {
  if (node_)
    return node_->ComputeColor();
  return SK_ColorBLACK;
}

SkColor AXPlatformNodeDelegate::GetBackgroundColor() const {
  if (node_)
    return node_->ComputeBackgroundColor();
  return SK_ColorWHITE;
}

bool AXPlatformNodeDelegate::IsReadOnlySupported() const {
  if (node_)
    return node_->IsReadOnlySupported();
  return false;
}

bool AXPlatformNodeDelegate::IsReadOnlyOrDisabled() const {
  if (node_)
    return node_->IsReadOnlyOrDisabled();
  return false;
}

std::string AXPlatformNodeDelegate::GetLanguage() const {
  if (node_)
    return node_->GetLanguage();
  return std::string();
}

}  // namespace ui
