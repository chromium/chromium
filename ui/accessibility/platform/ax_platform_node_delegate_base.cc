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
#include "ui/accessibility/platform/ax_platform_node_base.h"

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

ax::mojom::Role AXPlatformNodeDelegateBase::GetRole() const {
  return GetData().role;
}

bool AXPlatformNodeDelegateBase::HasBoolAttribute(
    ax::mojom::BoolAttribute attribute) const {
  return GetData().HasBoolAttribute(attribute);
}

bool AXPlatformNodeDelegateBase::GetBoolAttribute(
    ax::mojom::BoolAttribute attribute) const {
  return GetData().GetBoolAttribute(attribute);
}

bool AXPlatformNodeDelegateBase::GetBoolAttribute(
    ax::mojom::BoolAttribute attribute,
    bool* value) const {
  return GetData().GetBoolAttribute(attribute, value);
}

bool AXPlatformNodeDelegateBase::HasFloatAttribute(
    ax::mojom::FloatAttribute attribute) const {
  return GetData().HasFloatAttribute(attribute);
}

float AXPlatformNodeDelegateBase::GetFloatAttribute(
    ax::mojom::FloatAttribute attribute) const {
  return GetData().GetFloatAttribute(attribute);
}

bool AXPlatformNodeDelegateBase::GetFloatAttribute(
    ax::mojom::FloatAttribute attribute,
    float* value) const {
  return GetData().GetFloatAttribute(attribute, value);
}

const std::vector<std::pair<ax::mojom::IntAttribute, int32_t>>&
AXPlatformNodeDelegateBase::GetIntAttributes() const {
  return GetData().int_attributes;
}

bool AXPlatformNodeDelegateBase::HasIntAttribute(
    ax::mojom::IntAttribute attribute) const {
  return GetData().HasIntAttribute(attribute);
}

int AXPlatformNodeDelegateBase::GetIntAttribute(
    ax::mojom::IntAttribute attribute) const {
  return GetData().GetIntAttribute(attribute);
}

bool AXPlatformNodeDelegateBase::GetIntAttribute(
    ax::mojom::IntAttribute attribute,
    int* value) const {
  return GetData().GetIntAttribute(attribute, value);
}

const std::vector<std::pair<ax::mojom::StringAttribute, std::string>>&
AXPlatformNodeDelegateBase::GetStringAttributes() const {
  return GetData().string_attributes;
}

bool AXPlatformNodeDelegateBase::HasStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  return GetData().HasStringAttribute(attribute);
}

const std::string& AXPlatformNodeDelegateBase::GetStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  return GetData().GetStringAttribute(attribute);
}

bool AXPlatformNodeDelegateBase::GetStringAttribute(
    ax::mojom::StringAttribute attribute,
    std::string* value) const {
  return GetData().GetStringAttribute(attribute, value);
}

std::u16string AXPlatformNodeDelegateBase::GetString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  return GetData().GetString16Attribute(attribute);
}

bool AXPlatformNodeDelegateBase::GetString16Attribute(
    ax::mojom::StringAttribute attribute,
    std::u16string* value) const {
  return GetData().GetString16Attribute(attribute, value);
}

const std::string& AXPlatformNodeDelegateBase::GetInheritedStringAttribute(
    ax::mojom::StringAttribute attribute) const {
  NOTIMPLEMENTED();
  return GetData().GetStringAttribute(attribute);
}

std::u16string AXPlatformNodeDelegateBase::GetInheritedString16Attribute(
    ax::mojom::StringAttribute attribute) const {
  NOTIMPLEMENTED();
  return GetData().GetString16Attribute(attribute);
}

const std::vector<std::pair<ax::mojom::IntListAttribute, std::vector<int32_t>>>&
AXPlatformNodeDelegateBase::GetIntListAttributes() const {
  return GetData().intlist_attributes;
}

bool AXPlatformNodeDelegateBase::HasIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  return GetData().HasIntListAttribute(attribute);
}

const std::vector<int32_t>& AXPlatformNodeDelegateBase::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute) const {
  return GetData().GetIntListAttribute(attribute);
}

bool AXPlatformNodeDelegateBase::GetIntListAttribute(
    ax::mojom::IntListAttribute attribute,
    std::vector<int32_t>* value) const {
  return GetData().GetIntListAttribute(attribute, value);
}

bool AXPlatformNodeDelegateBase::HasStringListAttribute(
    ax::mojom::StringListAttribute attribute) const {
  return GetData().HasStringListAttribute(attribute);
}

const std::vector<std::string>&
AXPlatformNodeDelegateBase::GetStringListAttribute(
    ax::mojom::StringListAttribute attribute) const {
  return GetData().GetStringListAttribute(attribute);
}

bool AXPlatformNodeDelegateBase::GetStringListAttribute(
    ax::mojom::StringListAttribute attribute,
    std::vector<std::string>* value) const {
  return GetData().GetStringListAttribute(attribute, value);
}

bool AXPlatformNodeDelegateBase::HasHtmlAttribute(const char* attribute) const {
  return GetData().HasHtmlAttribute(attribute);
}

const base::StringPairs& AXPlatformNodeDelegateBase::GetHtmlAttributes() const {
  return GetData().html_attributes;
}

bool AXPlatformNodeDelegateBase::GetHtmlAttribute(const char* attribute,
                                                  std::string* value) const {
  return GetData().GetHtmlAttribute(attribute, value);
}

bool AXPlatformNodeDelegateBase::GetHtmlAttribute(const char* attribute,
                                                  std::u16string* value) const {
  return GetData().GetHtmlAttribute(attribute, value);
}

AXTextAttributes AXPlatformNodeDelegateBase::GetTextAttributes() const {
  return GetData().GetTextAttributes();
}

bool AXPlatformNodeDelegateBase::HasState(ax::mojom::State state) const {
  return GetData().HasState(state);
}

ax::mojom::State AXPlatformNodeDelegateBase::GetState() const {
  return static_cast<ax::mojom::State>(GetData().state);
}

bool AXPlatformNodeDelegateBase::HasAction(ax::mojom::Action action) const {
  return GetData().HasAction(action);
}

bool AXPlatformNodeDelegateBase::HasTextStyle(
    ax::mojom::TextStyle text_style) const {
  return GetData().HasTextStyle(text_style);
}

ax::mojom::NameFrom AXPlatformNodeDelegateBase::GetNameFrom() const {
  return GetData().GetNameFrom();
}

std::u16string AXPlatformNodeDelegateBase::GetTextContentUTF16() const {
  // Unlike in web content The "kValue" attribute always takes precedence,
  // because we assume that users of this base class, such as Views controls,
  // are carefully crafted by hand, in contrast to HTML pages, where any content
  // that might be present in the shadow DOM (AKA in the internal accessibility
  // tree) is actually used by the renderer when assigning the "kValue"
  // attribute, including any redundant white space.
  std::u16string value =
      GetString16Attribute(ax::mojom::StringAttribute::kValue);
  if (!value.empty())
    return value;

  // TODO(https://crbug.com/1030703): The check for IsInvisibleOrIgnored()
  // should not be needed. ChildAtIndex() and GetChildCount() are already
  // supposed to skip over nodes that are invisible or ignored, but
  // ViewAXPlatformNodeDelegate does not currently implement this behavior.
  if (IsLeaf() && !IsInvisibleOrIgnored())
    return GetString16Attribute(ax::mojom::StringAttribute::kName);

  std::u16string text_content;
  for (int i = 0; i < GetChildCount(); ++i) {
    // TODO(nektar): Add const to all tree traversal methods and remove
    // const_cast.
    const AXPlatformNode* child = AXPlatformNode::FromNativeViewAccessible(
        const_cast<AXPlatformNodeDelegateBase*>(this)->ChildAtIndex(i));
    if (!child || !child->GetDelegate())
      continue;
    text_content += child->GetDelegate()->GetTextContentUTF16();
  }
  return text_content;
}

std::u16string AXPlatformNodeDelegateBase::GetValueForControl() const {
  if (!IsControl(GetRole()) && !GetData().IsRangeValueSupported())
    return std::u16string();

  std::u16string value =
      GetString16Attribute(ax::mojom::StringAttribute::kValue);
  float numeric_value;
  if (GetData().IsRangeValueSupported() && value.empty() &&
      GetData().GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                                  &numeric_value)) {
    value = base::NumberToString16(numeric_value);
  }
  return value;
}

const AXTree::Selection AXPlatformNodeDelegateBase::GetUnignoredSelection()
    const {
  NOTIMPLEMENTED();
  return AXTree::Selection{false, -1, -1, ax::mojom::TextAffinity::kDownstream};
}

AXNodePosition::AXPositionInstance AXPlatformNodeDelegateBase::CreatePositionAt(
    int offset,
    ax::mojom::TextAffinity affinity) const {
  return AXNodePosition::CreateNullPosition();
}

AXNodePosition::AXPositionInstance
AXPlatformNodeDelegateBase::CreateTextPositionAt(
    int offset,
    ax::mojom::TextAffinity affinity) const {
  return AXNodePosition::CreateNullPosition();
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetNSWindow() {
  return nullptr;
}

gfx::NativeViewAccessible
AXPlatformNodeDelegateBase::GetNativeViewAccessible() {
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetParent() const {
  return nullptr;
}

int AXPlatformNodeDelegateBase::GetChildCount() const {
  return 0;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::ChildAtIndex(int index) {
  return nullptr;
}

bool AXPlatformNodeDelegateBase::HasModalDialog() const {
  return false;
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

bool AXPlatformNodeDelegateBase::IsChildOfLeaf() const {
  // TODO(nektar): Make all tree traversal methods const and remove const_cast.
  const AXPlatformNodeDelegate* parent =
      const_cast<AXPlatformNodeDelegateBase*>(this)->GetParentDelegate();
  if (!parent)
    return false;
  if (parent->IsLeaf())
    return true;
  return parent->IsChildOfLeaf();
}

bool AXPlatformNodeDelegateBase::IsLeaf() const {
  return !GetChildCount();
}

bool AXPlatformNodeDelegateBase::IsFocused() const {
  return false;
}

bool AXPlatformNodeDelegateBase::IsIgnored() const {
  // To avoid the situation where a screen reader user will not be able to
  // access a focused node because it has accidentally been marked as ignored,
  // we unignore any nodes that are focused. However, we don't need to check
  // this here because subclasses should make sure that the ignored state is
  // removed from all nodes that are currently focused. This condition will be
  // enforced once we switch to using an AXTree of AXNodes in Views.
  return GetRole() == ax::mojom::Role::kNone ||
         HasState(ax::mojom::State::kIgnored);
}

bool AXPlatformNodeDelegateBase::IsInvisibleOrIgnored() const {
  return IsIgnored() || GetData().IsInvisible();
}

bool AXPlatformNodeDelegateBase::IsToplevelBrowserWindow() {
  return false;
}

bool AXPlatformNodeDelegateBase::IsPlatformDocument() const {
  return ui::IsPlatformDocument(GetRole());
}

bool AXPlatformNodeDelegateBase::IsPlatformDocumentWithContent() const {
  return IsPlatformDocument() && GetChildCount();
}

bool AXPlatformNodeDelegateBase::IsDescendantOfAtomicTextField() const {
  // TODO(nektar): Add const to all tree traversal methods and remove
  // const_cast.
  for (AXPlatformNodeDelegateBase* ancestor_delegate =
           const_cast<AXPlatformNodeDelegateBase*>(this);
       ancestor_delegate;
       ancestor_delegate = static_cast<AXPlatformNodeDelegateBase*>(
           ancestor_delegate->GetParentDelegate())) {
    if (ancestor_delegate->GetData().IsAtomicTextField())
      return true;
  }
  return false;
}

gfx::NativeViewAccessible
AXPlatformNodeDelegateBase::GetLowestPlatformAncestor() const {
  AXPlatformNodeDelegateBase* current_delegate =
      const_cast<AXPlatformNodeDelegateBase*>(this);
  AXPlatformNodeDelegateBase* lowest_unignored_delegate = current_delegate;
  if (lowest_unignored_delegate->IsIgnored()) {
    lowest_unignored_delegate = static_cast<AXPlatformNodeDelegateBase*>(
        lowest_unignored_delegate->GetParentDelegate());
  }
  DCHECK(!lowest_unignored_delegate || !lowest_unignored_delegate->IsIgnored())
      << "`AXPlatformNodeDelegateBase::GetParentDelegate()` should return "
         "either an unignored object or nullptr.";

  // `highest_leaf_delegate` could be nullptr.
  AXPlatformNodeDelegateBase* highest_leaf_delegate = lowest_unignored_delegate;
  // For the purposes of this method, a leaf node does not include leaves in the
  // internal accessibility tree, only in the platform exposed tree.
  for (AXPlatformNodeDelegateBase* ancestor_delegate =
           lowest_unignored_delegate;
       ancestor_delegate;
       ancestor_delegate = static_cast<AXPlatformNodeDelegateBase*>(
           ancestor_delegate->GetParentDelegate())) {
    if (ancestor_delegate->IsLeaf())
      highest_leaf_delegate = ancestor_delegate;
  }
  if (highest_leaf_delegate)
    return highest_leaf_delegate->GetNativeViewAccessible();

  if (lowest_unignored_delegate)
    return lowest_unignored_delegate->GetNativeViewAccessible();
  return current_delegate->GetNativeViewAccessible();
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetTextFieldAncestor()
    const {
  // TODO(nektar): Add const to all tree traversal methods and remove
  // const_cast.
  for (AXPlatformNodeDelegateBase* ancestor_delegate =
           const_cast<AXPlatformNodeDelegateBase*>(this);
       ancestor_delegate;
       ancestor_delegate = static_cast<AXPlatformNodeDelegateBase*>(
           ancestor_delegate->GetParentDelegate())) {
    if (ancestor_delegate->GetData().IsTextField())
      return ancestor_delegate->GetNativeViewAccessible();
  }
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetSelectionContainer()
    const {
  // TODO(nektar): Add const to all tree traversal methods and remove
  // const_cast.
  for (AXPlatformNodeDelegateBase* ancestor_delegate =
           const_cast<AXPlatformNodeDelegateBase*>(this);
       ancestor_delegate;
       ancestor_delegate = static_cast<AXPlatformNodeDelegateBase*>(
           ancestor_delegate->GetParentDelegate())) {
    if (IsContainerWithSelectableChildren(ancestor_delegate->GetRole()))
      return ancestor_delegate->GetNativeViewAccessible();
  }
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetTableAncestor() const {
  // TODO(nektar): Add const to all tree traversal methods and remove
  // const_cast.
  for (AXPlatformNodeDelegateBase* ancestor_delegate =
           const_cast<AXPlatformNodeDelegateBase*>(this);
       ancestor_delegate;
       ancestor_delegate = static_cast<AXPlatformNodeDelegateBase*>(
           ancestor_delegate->GetParentDelegate())) {
    if (IsTableLike(ancestor_delegate->GetRole()))
      return ancestor_delegate->GetNativeViewAccessible();
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

AXPlatformNodeDelegateBase::ChildIteratorBase&
AXPlatformNodeDelegateBase::ChildIteratorBase::operator++() {
  index_++;
  return *this;
}

AXPlatformNodeDelegateBase::ChildIteratorBase&
AXPlatformNodeDelegateBase::ChildIteratorBase::operator++(int) {
  index_++;
  return *this;
}

AXPlatformNodeDelegateBase::ChildIteratorBase&
AXPlatformNodeDelegateBase::ChildIteratorBase::operator--() {
  DCHECK_GT(index_, 0);
  index_--;
  return *this;
}

AXPlatformNodeDelegateBase::ChildIteratorBase&
AXPlatformNodeDelegateBase::ChildIteratorBase::operator--(int) {
  DCHECK_GT(index_, 0);
  index_--;
  return *this;
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

AXPlatformNodeDelegate&
AXPlatformNodeDelegateBase::ChildIteratorBase::operator*() const {
  AXPlatformNode* platform_node =
      AXPlatformNode::FromNativeViewAccessible(GetNativeViewAccessible());
  DCHECK(platform_node && platform_node->GetDelegate());
  return *(platform_node->GetDelegate());
}

AXPlatformNodeDelegate*
AXPlatformNodeDelegateBase::ChildIteratorBase::operator->() const {
  AXPlatformNode* platform_node =
      AXPlatformNode::FromNativeViewAccessible(GetNativeViewAccessible());
  return platform_node ? platform_node->GetDelegate() : nullptr;
}

std::unique_ptr<AXPlatformNodeDelegate::ChildIterator>
AXPlatformNodeDelegateBase::ChildrenBegin() {
  return std::make_unique<ChildIteratorBase>(this, 0);
}

std::unique_ptr<AXPlatformNodeDelegate::ChildIterator>
AXPlatformNodeDelegateBase::ChildrenEnd() {
  return std::make_unique<ChildIteratorBase>(this, GetChildCount());
}

const std::string& AXPlatformNodeDelegateBase::GetName() const {
  return GetStringAttribute(ax::mojom::StringAttribute::kName);
}

std::u16string AXPlatformNodeDelegateBase::GetHypertext() const {
  return std::u16string();
}

const std::map<int, int>&
AXPlatformNodeDelegateBase::GetHypertextOffsetToHyperlinkChildIndex() const {
  // TODO(nektar): Remove this dummy method once hypertext computation and
  // selection handling has moved entirely to AXNode / AXPosition.
  static base::NoDestructor<std::map<int, int>> dummy_map;
  return *dummy_map;
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

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::HitTestSync(
    int screen_physical_pixel_x,
    int screen_physical_pixel_y) const {
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegateBase::GetFocus() const {
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
  return ui::IsTableLike(GetRole());
}

absl::optional<int> AXPlatformNodeDelegateBase::GetTableRowCount() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kTableRowCount);
}

absl::optional<int> AXPlatformNodeDelegateBase::GetTableColCount() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kTableColumnCount);
}

absl::optional<int> AXPlatformNodeDelegateBase::GetTableAriaColCount() const {
  int aria_column_count;
  if (!GetIntAttribute(ax::mojom::IntAttribute::kAriaColumnCount,
                       &aria_column_count)) {
    return absl::nullopt;
  }
  return aria_column_count;
}

absl::optional<int> AXPlatformNodeDelegateBase::GetTableAriaRowCount() const {
  int aria_row_count;
  if (!GetIntAttribute(ax::mojom::IntAttribute::kAriaRowCount,
                       &aria_row_count)) {
    return absl::nullopt;
  }
  return aria_row_count;
}

absl::optional<int> AXPlatformNodeDelegateBase::GetTableCellCount() const {
  return absl::nullopt;
}

absl::optional<bool>
AXPlatformNodeDelegateBase::GetTableHasColumnOrRowHeaderNode() const {
  return absl::nullopt;
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
  return ui::IsTableRow(GetRole());
}

absl::optional<int> AXPlatformNodeDelegateBase::GetTableRowRowIndex() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kTableRowIndex);
}

bool AXPlatformNodeDelegateBase::IsTableCellOrHeader() const {
  return ui::IsCellOrTableHeader(GetRole());
}

absl::optional<int> AXPlatformNodeDelegateBase::GetTableCellColIndex() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex);
}

absl::optional<int> AXPlatformNodeDelegateBase::GetTableCellRowIndex() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex);
}

absl::optional<int> AXPlatformNodeDelegateBase::GetTableCellColSpan() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan);
}

absl::optional<int> AXPlatformNodeDelegateBase::GetTableCellRowSpan() const {
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan);
}

absl::optional<int> AXPlatformNodeDelegateBase::GetTableCellAriaColIndex()
    const {
  if (HasIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnIndex)) {
    return GetIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnIndex);
  }

  return absl::nullopt;
}

absl::optional<int> AXPlatformNodeDelegateBase::GetTableCellAriaRowIndex()
    const {
  if (HasIntAttribute(ax::mojom::IntAttribute::kAriaCellRowIndex)) {
    return GetIntAttribute(ax::mojom::IntAttribute::kAriaCellRowIndex);
  }

  return absl::nullopt;
}

absl::optional<int32_t> AXPlatformNodeDelegateBase::GetCellId(
    int row_index,
    int col_index) const {
  return absl::nullopt;
}

absl::optional<int> AXPlatformNodeDelegateBase::GetTableCellIndex() const {
  return absl::nullopt;
}

absl::optional<int32_t> AXPlatformNodeDelegateBase::CellIndexToId(
    int cell_index) const {
  return absl::nullopt;
}

bool AXPlatformNodeDelegateBase::IsCellOrHeaderOfAriaGrid() const {
  return false;
}

bool AXPlatformNodeDelegateBase::IsWebAreaForPresentationalIframe() const {
  if (!ui::IsPlatformDocument(GetRole()))
    return false;

  AXPlatformNodeDelegate* parent = GetParentDelegate();
  if (!parent)
    return false;

  return parent->GetRole() == ax::mojom::Role::kIframePresentational;
}

bool AXPlatformNodeDelegateBase::IsOrderedSetItem() const {
  return false;
}

bool AXPlatformNodeDelegateBase::IsOrderedSet() const {
  return false;
}

absl::optional<int> AXPlatformNodeDelegateBase::GetPosInSet() const {
  return absl::nullopt;
}

absl::optional<int> AXPlatformNodeDelegateBase::GetSetSize() const {
  return absl::nullopt;
}

SkColor AXPlatformNodeDelegateBase::GetColor() const {
  return SK_ColorBLACK;
}

SkColor AXPlatformNodeDelegateBase::GetBackgroundColor() const {
  return SK_ColorWHITE;
}

bool AXPlatformNodeDelegateBase::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  return false;
}

std::u16string
AXPlatformNodeDelegateBase::GetLocalizedStringForImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) const {
  return std::u16string();
}

std::u16string
AXPlatformNodeDelegateBase::GetLocalizedRoleDescriptionForUnlabeledImage()
    const {
  return std::u16string();
}

std::u16string AXPlatformNodeDelegateBase::GetLocalizedStringForLandmarkType()
    const {
  return std::u16string();
}

std::u16string
AXPlatformNodeDelegateBase::GetLocalizedStringForRoleDescription() const {
  return std::u16string();
}

std::u16string
AXPlatformNodeDelegateBase::GetStyleNameAttributeAsLocalizedString() const {
  return std::u16string();
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

bool AXPlatformNodeDelegateBase::IsText() const {
  return ui::IsText(GetRole());
}

bool AXPlatformNodeDelegateBase::IsWebContent() const {
  return false;
}

bool AXPlatformNodeDelegateBase::HasVisibleCaretOrSelection() const {
  return IsDescendantOfAtomicTextField();
}

AXPlatformNode* AXPlatformNodeDelegateBase::GetTargetNodeForRelation(
    ax::mojom::IntAttribute attr) {
  DCHECK(IsNodeIdIntAttribute(attr));

  int target_id;
  if (!GetIntAttribute(attr, &target_id))
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

std::vector<AXPlatformNode*>
AXPlatformNodeDelegateBase::GetTargetNodesForRelation(
    ax::mojom::IntListAttribute attr) {
  DCHECK(IsNodeIdIntListAttribute(attr));
  std::vector<int32_t> target_ids;
  if (!GetIntListAttribute(attr, &target_ids))
    return std::vector<AXPlatformNode*>();

  // If we use std::set to eliminate duplicates, the resulting set will be
  // sorted by the id and we will lose the original order which may be of
  // interest to ATs. The number of ids should be small.

  std::vector<ui::AXPlatformNode*> nodes;
  for (int32_t target_id : target_ids) {
    if (ui::AXPlatformNode* node = GetFromNodeID(target_id)) {
      if (std::find(nodes.begin(), nodes.end(), node) == nodes.end())
        nodes.push_back(node);
    }
  }

  return nodes;
}

std::set<AXPlatformNode*> AXPlatformNodeDelegateBase::GetReverseRelations(
    ax::mojom::IntAttribute attr) {
  // TODO(accessibility) Implement these if views ever use relations more
  // widely. The use so far has been for the Omnibox to the suggestion popup.
  // If this is ever implemented, then the "popup for" to "controlled by"
  // mapping in AXPlatformRelationWin can be removed, as it would be
  // redundant with setting the controls relationship.
  return std::set<AXPlatformNode*>();
}

std::set<AXPlatformNode*> AXPlatformNodeDelegateBase::GetReverseRelations(
    ax::mojom::IntListAttribute attr) {
  return std::set<AXPlatformNode*>();
}

std::u16string AXPlatformNodeDelegateBase::GetAuthorUniqueId() const {
  return std::u16string();
}

const AXUniqueId& AXPlatformNodeDelegateBase::GetUniqueId() const {
  static base::NoDestructor<AXUniqueId> dummy_unique_id;
  return *dummy_unique_id;
}

const std::vector<gfx::NativeViewAccessible>
AXPlatformNodeDelegateBase::GetUIADirectChildrenInRange(
    ui::AXPlatformNodeDelegate* start,
    ui::AXPlatformNodeDelegate* end) {
  return {};
}

std::string AXPlatformNodeDelegateBase::GetLanguage() const {
  return std::string();
}

AXPlatformNodeDelegate* AXPlatformNodeDelegateBase::GetParentDelegate() const {
  AXPlatformNode* parent_node =
      ui::AXPlatformNode::FromNativeViewAccessible(GetParent());
  if (parent_node)
    return parent_node->GetDelegate();

  return nullptr;
}

std::string AXPlatformNodeDelegateBase::SubtreeToStringHelper(size_t level) {
  std::string result(level * 2, '+');
  result += ToString();
  result += '\n';

  // We can't use ChildrenBegin() and ChildrenEnd() here, because they both
  // return an std::unique_ptr<ChildIterator> which is an abstract class.
  //
  // TODO(accessibility): Refactor ChildIterator into a separate base
  // (non-abstract) class.
  auto iter_start = ChildIteratorBase(this, 0);
  auto iter_end = ChildIteratorBase(this, GetChildCount());
  for (auto iter = iter_start; iter != iter_end; ++iter) {
    AXPlatformNodeDelegateBase& child =
        static_cast<AXPlatformNodeDelegateBase&>(*iter);
    result += child.SubtreeToStringHelper(level + 1);
  }

  return result;
}

}  // namespace ui
