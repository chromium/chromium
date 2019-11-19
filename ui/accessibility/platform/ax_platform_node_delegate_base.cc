// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"

#include <vector>

#include "base/no_destructor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_constants.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"

namespace ui {

AXPlatformNodeDelegateBase::AXPlatformNodeDelegateBase() = default;

AXPlatformNodeDelegateBase::~AXPlatformNodeDelegateBase() = default;

const AXNodeData& AXPlatformNodeDelegateBase::GetData() const {
  static base::NoDestructor<AXNodeData> empty_data;
  return *empty_data;
}

const AXTreeData& AXPlatformNodeDelegateBase::GetTreeData() const {
  static base::NoDestructor<AXTreeData> empty_data;
  return *empty_data;
}

const AXTree::Selection AXPlatformNodeDelegateBase::GetUnignoredSelection()
    const {
  return AXTree::Selection{-1, -1, -1, ax::mojom::TextAffinity::kDownstream};
}

AXNodePosition::AXPositionInstance
AXPlatformNodeDelegateBase::CreateTextPositionAt(int offset) const {
  return AXNodePosition::CreateNullPosition();
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetNSWindow() {
  return nullptr;
}

gfx::NativeViewAccessible
AXPlatformNodeDelegateBase::GetNativeViewAccessible() {
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetParent() {
  return nullptr;
}

int AXPlatformNodeDelegateBase::GetChildCount() {
  return 0;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::ChildAtIndex(int index) {
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetFirstChild() {
  if (GetChildCount() > 0)
    return ChildAtIndex(0);
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetLastChild() {
  if (GetChildCount() > 0)
    return ChildAtIndex(GetChildCount() - 1);
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetNextSibling() {
  AXPlatformNodeDelegate* parent = GetParentDelegate();
  if (parent && GetIndexInParent() >= 0) {
    int next_index = GetIndexInParent() + 1;
    if (next_index >= 0 && next_index < parent->GetChildCount())
      return parent->ChildAtIndex(next_index);
  }
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetPreviousSibling() {
  AXPlatformNodeDelegate* parent = GetParentDelegate();
  if (parent && GetIndexInParent() >= 0) {
    int next_index = GetIndexInParent() - 1;
    if (next_index >= 0 && next_index < parent->GetChildCount())
      return parent->ChildAtIndex(next_index);
  }
  return nullptr;
}

AXPlatformNodeDelegateBase::ChildIteratorBase::ChildIteratorBase(
    AXPlatformNodeDelegateBase* parent,
    int index)
    : index_(index), parent_(parent) {
  DCHECK(parent);
  DCHECK(0 <= index && index <= parent->GetChildCount());
}

AXPlatformNodeDelegateBase::ChildIteratorBase::ChildIteratorBase(
    const AXPlatformNodeDelegateBase::ChildIteratorBase& it)
    : index_(it.index_), parent_(it.parent_) {
  DCHECK(parent_);
}

bool AXPlatformNodeDelegateBase::ChildIteratorBase::operator==(
    const AXPlatformNodeDelegate::ChildIterator& rhs) const {
  return rhs.GetIndexInParent() == index_;
}

bool AXPlatformNodeDelegateBase::ChildIteratorBase::operator!=(
    const AXPlatformNodeDelegate::ChildIterator& rhs) const {
  return rhs.GetIndexInParent() != index_;
}

void AXPlatformNodeDelegateBase::ChildIteratorBase::operator++() {
  index_++;
}

void AXPlatformNodeDelegateBase::ChildIteratorBase::operator++(int) {
  index_++;
}

void AXPlatformNodeDelegateBase::ChildIteratorBase::operator--() {
  DCHECK_GT(index_, 0);
  index_--;
}

void AXPlatformNodeDelegateBase::ChildIteratorBase::operator--(int) {
  DCHECK_GT(index_, 0);
  index_--;
}

gfx::NativeViewAccessible
AXPlatformNodeDelegateBase::ChildIteratorBase::GetNativeViewAccessible() const {
  if (index_ < parent_->GetChildCount())
    return parent_->ChildAtIndex(index_);

  return nullptr;
}

int AXPlatformNodeDelegateBase::ChildIteratorBase::GetIndexInParent() const {
  return index_;
}

std::unique_ptr<AXPlatformNodeDelegate::ChildIterator>
AXPlatformNodeDelegateBase::ChildrenBegin() {
  return std::make_unique<ChildIteratorBase>(this, 0);
}

std::unique_ptr<AXPlatformNodeDelegate::ChildIterator>
AXPlatformNodeDelegateBase::ChildrenEnd() {
  return std::make_unique<ChildIteratorBase>(this, GetChildCount());
}

base::string16 AXPlatformNodeDelegateBase::GetHypertext() const {
  return base::string16();
}

bool AXPlatformNodeDelegateBase::SetHypertextSelection(int start_offset,
                                                       int end_offset) {
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kSetSelection;
  action_data.anchor_node_id = action_data.focus_node_id = GetData().id;
  action_data.anchor_offset = start_offset;
  action_data.focus_offset = end_offset;
  return AccessibilityPerformAction(action_data);
}

base::string16 AXPlatformNodeDelegateBase::GetInnerText() const {
  return base::string16();
}

gfx::Rect AXPlatformNodeDelegateBase::GetBoundsRect(
    const AXCoordinateSystem coordinate_system,
    const AXClippingBehavior clipping_behavior,
    AXOffscreenResult* offscreen_result) const {
  return gfx::Rect();
}

gfx::Rect AXPlatformNodeDelegateBase::GetHypertextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    const AXCoordinateSystem coordinate_system,
    const AXClippingBehavior clipping_behavior,
    AXOffscreenResult* offscreen_result) const {
  return gfx::Rect();
}

gfx::Rect AXPlatformNodeDelegateBase::GetInnerTextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    const AXCoordinateSystem coordinate_system,
    const AXClippingBehavior clipping_behavior,
    AXOffscreenResult* offscreen_result = nullptr) const {
  return gfx::Rect();
}

gfx::Rect AXPlatformNodeDelegateBase::GetClippedScreenBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kScreen,
                       AXClippingBehavior::kClipped, offscreen_result);
}

gfx::Rect AXPlatformNodeDelegateBase::GetUnclippedScreenBoundsRect(
    AXOffscreenResult* offscreen_result) const {
  return GetBoundsRect(AXCoordinateSystem::kScreen,
                       AXClippingBehavior::kUnclipped, offscreen_result);
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::HitTestSync(int x,
                                                                  int y) {
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetFocus() {
  return nullptr;
}

AXPlatformNode* AXPlatformNodeDelegateBase::GetFromNodeID(int32_t id) {
  return nullptr;
}

AXPlatformNode* AXPlatformNodeDelegateBase::GetFromTreeIDAndNodeID(
    const ui::AXTreeID& ax_tree_id,
    int32_t id) {
  return nullptr;
}

int AXPlatformNodeDelegateBase::GetIndexInParent() {
  AXPlatformNodeDelegate* parent = GetParentDelegate();
  if (!parent)
    return -1;

  for (int i = 0; i < parent->GetChildCount(); i++) {
    AXPlatformNode* child_node =
        AXPlatformNode::FromNativeViewAccessible(parent->ChildAtIndex(i));
    if (child_node && child_node->GetDelegate() == this)
      return i;
  }
  return -1;
}

gfx::AcceleratedWidget
AXPlatformNodeDelegateBase::GetTargetForNativeAccessibilityEvent() {
  return gfx::kNullAcceleratedWidget;
}

bool AXPlatformNodeDelegateBase::IsTable() const {
  return ui::IsTableLike(GetData().role);
}

base::Optional<int> AXPlatformNodeDelegateBase::GetTableRowCount() const {
  return GetData().GetIntAttribute(ax::mojom::IntAttribute::kTableRowCount);
}

base::Optional<int> AXPlatformNodeDelegateBase::GetTableColCount() const {
  return GetData().GetIntAttribute(ax::mojom::IntAttribute::kTableColumnCount);
}

base::Optional<int> AXPlatformNodeDelegateBase::GetTableAriaColCount() const {
  int aria_column_count =
      GetData().GetIntAttribute(ax::mojom::IntAttribute::kAriaColumnCount);
  if (aria_column_count == ax::mojom::kUnknownAriaColumnOrRowCount)
    return base::nullopt;
  return aria_column_count;
}

base::Optional<int> AXPlatformNodeDelegateBase::GetTableAriaRowCount() const {
  int aria_row_count =
      GetData().GetIntAttribute(ax::mojom::IntAttribute::kAriaRowCount);
  if (aria_row_count == ax::mojom::kUnknownAriaColumnOrRowCount)
    return base::nullopt;
  return aria_row_count;
}

base::Optional<int> AXPlatformNodeDelegateBase::GetTableCellCount() const {
  return base::nullopt;
}

std::vector<int32_t> AXPlatformNodeDelegateBase::GetColHeaderNodeIds() const {
  return {};
}

std::vector<int32_t> AXPlatformNodeDelegateBase::GetColHeaderNodeIds(
    int col_index) const {
  return {};
}

std::vector<int32_t> AXPlatformNodeDelegateBase::GetRowHeaderNodeIds() const {
  return {};
}

std::vector<int32_t> AXPlatformNodeDelegateBase::GetRowHeaderNodeIds(
    int row_index) const {
  return {};
}

AXPlatformNode* AXPlatformNodeDelegateBase::GetTableCaption() const {
  return nullptr;
}

bool AXPlatformNodeDelegateBase::IsTableRow() const {
  return ui::IsTableRow(GetData().role);
}

base::Optional<int> AXPlatformNodeDelegateBase::GetTableRowRowIndex() const {
  return GetData().GetIntAttribute(ax::mojom::IntAttribute::kTableRowIndex);
}

bool AXPlatformNodeDelegateBase::IsTableCellOrHeader() const {
  return ui::IsCellOrTableHeader(GetData().role);
}

base::Optional<int> AXPlatformNodeDelegateBase::GetTableCellColIndex() const {
  return GetData().GetIntAttribute(
      ax::mojom::IntAttribute::kTableCellColumnIndex);
}

base::Optional<int> AXPlatformNodeDelegateBase::GetTableCellRowIndex() const {
  return GetData().GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex);
}

base::Optional<int> AXPlatformNodeDelegateBase::GetTableCellColSpan() const {
  return GetData().GetIntAttribute(
      ax::mojom::IntAttribute::kTableCellColumnSpan);
}

base::Optional<int> AXPlatformNodeDelegateBase::GetTableCellRowSpan() const {
  return GetData().GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan);
}

base::Optional<int> AXPlatformNodeDelegateBase::GetTableCellAriaColIndex()
    const {
  return GetData().GetIntAttribute(
      ax::mojom::IntAttribute::kAriaCellColumnIndex);
}

base::Optional<int> AXPlatformNodeDelegateBase::GetTableCellAriaRowIndex()
    const {
  return GetData().GetIntAttribute(ax::mojom::IntAttribute::kAriaCellRowIndex);
}

base::Optional<int32_t> AXPlatformNodeDelegateBase::GetCellId(
    int row_index,
    int col_index) const {
  return base::nullopt;
}

base::Optional<int> AXPlatformNodeDelegateBase::GetTableCellIndex() const {
  return base::nullopt;
}

base::Optional<int32_t> AXPlatformNodeDelegateBase::CellIndexToId(
    int cell_index) const {
  return base::nullopt;
}

bool AXPlatformNodeDelegateBase::IsCellOrHeaderOfARIATable() const {
  return false;
}

bool AXPlatformNodeDelegateBase::IsCellOrHeaderOfARIAGrid() const {
  return false;
}

bool AXPlatformNodeDelegateBase::IsOrderedSetItem() const {
  return false;
}

bool AXPlatformNodeDelegateBase::IsOrderedSet() const {
  return false;
}

base::Optional<int> AXPlatformNodeDelegateBase::GetPosInSet() const {
  return base::nullopt;
}

base::Optional<int> AXPlatformNodeDelegateBase::GetSetSize() const {
  return base::nullopt;
}

bool AXPlatformNodeDelegateBase::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  return false;
}

base::string16
AXPlatformNodeDelegateBase::GetLocalizedStringForImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) const {
  return base::string16();
}

base::string16
AXPlatformNodeDelegateBase::GetLocalizedRoleDescriptionForUnlabeledImage()
    const {
  return base::string16();
}

base::string16 AXPlatformNodeDelegateBase::GetLocalizedStringForLandmarkType()
    const {
  return base::string16();
}

base::string16
AXPlatformNodeDelegateBase::GetLocalizedStringForRoleDescription() const {
  return base::string16();
}

base::string16
AXPlatformNodeDelegateBase::GetStyleNameAttributeAsLocalizedString() const {
  return base::string16();
}

TextAttributeMap AXPlatformNodeDelegateBase::ComputeTextAttributeMap(
    const TextAttributeList& default_attributes) const {
  ui::TextAttributeMap attributes_map;
  attributes_map[0] = default_attributes;
  return attributes_map;
}

std::string AXPlatformNodeDelegateBase::GetInheritedFontFamilyName() const {
  // We don't have access to AXNodeData here, so we cannot return
  // an inherited font family name.
  return std::string();
}

bool AXPlatformNodeDelegateBase::ShouldIgnoreHoveredStateForTesting() {
  return true;
}

bool AXPlatformNodeDelegateBase::IsOffscreen() const {
  return false;
}

bool AXPlatformNodeDelegateBase::IsMinimized() const {
  return false;
}

bool AXPlatformNodeDelegateBase::IsWebContent() const {
  return false;
}

bool AXPlatformNodeDelegateBase::HasVisibleCaretOrSelection() const {
  return false;
}

AXPlatformNode* AXPlatformNodeDelegateBase::GetTargetNodeForRelation(
    ax::mojom::IntAttribute attr) {
  DCHECK(IsNodeIdIntAttribute(attr));

  int target_id;
  if (!GetData().GetIntAttribute(attr, &target_id))
    return nullptr;

  return GetFromNodeID(target_id);
}

std::set<AXPlatformNode*> AXPlatformNodeDelegateBase::GetNodesForNodeIds(
    const std::set<int32_t>& ids) {
  std::set<AXPlatformNode*> nodes;
  for (int32_t node_id : ids) {
    if (AXPlatformNode* node = GetFromNodeID(node_id)) {
      nodes.insert(node);
    }
  }
  return nodes;
}

std::set<AXPlatformNode*> AXPlatformNodeDelegateBase::GetTargetNodesForRelation(
    ax::mojom::IntListAttribute attr) {
  DCHECK(IsNodeIdIntListAttribute(attr));
  std::vector<int32_t> target_ids;
  if (!GetData().GetIntListAttribute(attr, &target_ids))
    return std::set<AXPlatformNode*>();

  std::set<int32_t> target_id_set(target_ids.begin(), target_ids.end());
  return GetNodesForNodeIds(target_id_set);
}

std::set<AXPlatformNode*> AXPlatformNodeDelegateBase::GetReverseRelations(
    ax::mojom::IntAttribute attr) {
  return std::set<AXPlatformNode*>();
}

std::set<AXPlatformNode*> AXPlatformNodeDelegateBase::GetReverseRelations(
    ax::mojom::IntListAttribute attr) {
  return std::set<AXPlatformNode*>();
}

base::string16 AXPlatformNodeDelegateBase::GetAuthorUniqueId() const {
  return base::string16();
}

const AXUniqueId& AXPlatformNodeDelegateBase::GetUniqueId() const {
  static base::NoDestructor<AXUniqueId> dummy_unique_id;
  return *dummy_unique_id;
}

base::Optional<int> AXPlatformNodeDelegateBase::FindTextBoundary(
    AXTextBoundary boundary,
    int offset,
    AXTextBoundaryDirection direction,
    ax::mojom::TextAffinity affinity) const {
  return base::nullopt;
}

const std::vector<gfx::NativeViewAccessible>
AXPlatformNodeDelegateBase::GetDescendants() const {
  return {};
}

std::string AXPlatformNodeDelegateBase::GetLanguage() const {
  return std::string();
}

AXPlatformNodeDelegate* AXPlatformNodeDelegateBase::GetParentDelegate() {
  AXPlatformNode* parent_node =
      ui::AXPlatformNode::FromNativeViewAccessible(GetParent());
  if (parent_node)
    return parent_node->GetDelegate();

  return nullptr;
}

}  // namespace ui
