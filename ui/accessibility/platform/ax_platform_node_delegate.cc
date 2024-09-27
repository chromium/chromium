// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_delegate.h"

#include "base/containers/fixed_flat_set.h"
#include "base/notreached.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/accessibility/platform/child_iterator.h"
#include "ui/accessibility/platform/child_iterator_base.h"

namespace ui {

AXPlatformNodeDelegate::AXPlatformNodeDelegate() : node_(nullptr) {}

AXPlatformNodeDelegate::AXPlatformNodeDelegate(AXNode* node) : node_(node) {
  DCHECK(node);
  DCHECK(node->IsDataValid());
}

void AXPlatformNodeDelegate::SetNode(AXNode& node) {
  DCHECK(node.IsDataValid());
  node_ = &node;
}

AXNodeID AXPlatformNodeDelegate::GetId() const {
  if (node_)
    return node_->id();
  return kInvalidAXNodeID;
}

AXTreeManager* AXPlatformNodeDelegate::GetTreeManager() const {
  return AXTreeManager::FromID(GetTreeData().tree_id);
}

const AXNodeData& AXPlatformNodeDelegate::GetData() const {
  if (node_)
    return node_->data();

  static base::NoDestructor<AXNodeData> empty_data;
  return *empty_data;
}

std::u16string AXPlatformNodeDelegate::GetTextContentUTF16() const {
  if (node_)
    return node_->GetTextContentUTF16();

  // Unlike in web content the "kValue" attribute always takes precedence,
  // because we assume that users of the base impl, such as Views controls,
  // are carefully crafted by hand, in contrast to HTML pages, where any content
  // that might be present in the shadow DOM (AKA in the internal accessibility
  // tree) is actually used by the renderer when assigning the "kValue"
  // attribute, including any redundant white space.
  std::u16string value =
      GetString16Attribute(ax::mojom::StringAttribute::kValue);
  if (!value.empty())
    return value;

  // The name of a leaf node in Views is displayed inside the View, i.e.
  // `GetNameFrom` == `ax::mojom::NameFrom::kContents`, except in text fields,
  // where the name attribute is the field's label and the value attribute is
  // the field's text contents. For maximum compatibility with the Web code, we
  // compute the text of a non-leaf text field from the text contents of its
  // children, even though we currently know of no such text field in Views.
  //
  // TODO(crbug.com/40662009): The check for `IsInvisibleOrIgnored()`
  // should not be needed. `ChildAtIndex()` and `GetChildCount()` are already
  // supposed to skip over nodes that are invisible or ignored, but
  // `ViewAXPlatformNodeDelegate` does not currently implement this behavior.
  if (IsLeaf() && !GetData().IsTextField() && !IsInvisibleOrIgnored()) {
    return GetString16Attribute(ax::mojom::StringAttribute::kName);
  }

  std::u16string text_content;
  for (size_t i = 0; i < GetChildCount(); ++i) {
    // TODO(nektar): Add const to all tree traversal methods and remove
    // const_cast.
    const AXPlatformNode* child = AXPlatformNode::FromNativeViewAccessible(
        const_cast<AXPlatformNodeDelegate*>(this)->ChildAtIndex(i));
    if (!child || !child->GetDelegate())
      continue;
    text_content += child->GetDelegate()->GetTextContentUTF16();
  }
  return text_content;
}

int AXPlatformNodeDelegate::GetTextContentLengthUTF16() const {
  // TODO(accessibility): Simplify once ViewsAX is complete.
  if (node_) {
    return node_->GetTextContentLengthUTF16();
  }

  return GetTextContentUTF16().length();
}

std::u16string AXPlatformNodeDelegate::GetValueForControl() const {
  if (node_)
    return base::UTF8ToUTF16(node()->GetValueForControl());

  if (!IsControl(GetRole()) && !GetData().IsRangeValueSupported())
    return std::u16string();

  std::u16string value =
      GetString16Attribute(ax::mojom::StringAttribute::kValue);
  if (GetData().IsRangeValueSupported() && value.empty()) {
    float numeric_value =
        GetData().GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange);
    if (numeric_value || GetData().HasFloatAttribute(
                             ax::mojom::FloatAttribute::kValueForRange)) {
      value = base::NumberToString16(numeric_value);
    }
  }
  return value;
}

AXNodePosition::AXPositionInstance AXPlatformNodeDelegate::CreatePositionAt(
    int offset,
    ax::mojom::TextAffinity affinity) const {
  if (node_)
    return AXNodePosition::CreatePosition(*node_, offset, affinity);
  return AXNodePosition::CreateNullPosition();
}

AXNodePosition::AXPositionInstance AXPlatformNodeDelegate::CreateTextPositionAt(
    int offset,
    ax::mojom::TextAffinity affinity) const {
  if (node_) {
    DCHECK(node_->tree())
        << "All nodes should be owned by an accessibility tree.\n"
        << *node_;
    DCHECK(node_->IsDataValid());
    return AXNodePosition::CreateTextPosition(*node_, offset, affinity);
  }
  return AXNodePosition::CreateNullPosition();
}

gfx::NativeViewAccessible AXPlatformNodeDelegate::GetNSWindow() {
  NOTREACHED_IN_MIGRATION() << "Only available on macOS.";
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegate::GetNativeViewAccessible() {
  // TODO(crbug.com/41308426) On Windows, where we have started to migrate to an
  // AXPlatformNode implementation, the BrowserAccessibilityWin subclass has
  // overridden this method. On all other platforms, this method should not be
  // called yet. In the future, when all subclasses have moved over to be
  // implemented by AXPlatformNode, we may make this method completely virtual.
  NOTREACHED_IN_MIGRATION() << "https://crbug.com/703369";
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegate::GetParent() const {
  return nullptr;
}

std::optional<size_t> AXPlatformNodeDelegate::GetIndexInParent() const {
  if (node_)
    return node_->GetUnignoredIndexInParent();

  AXPlatformNodeDelegate* parent = GetParentDelegate();
  if (!parent)
    return std::nullopt;

  for (size_t i = 0; i < parent->GetChildCount(); i++) {
    AXPlatformNode* child_node =
        AXPlatformNode::FromNativeViewAccessible(parent->ChildAtIndex(i));
    if (child_node && child_node->GetDelegate() == this)
      return i;
  }
  return std::nullopt;
}

size_t AXPlatformNodeDelegate::GetChildCount() const {
  return 0u;
}

gfx::NativeViewAccessible AXPlatformNodeDelegate::ChildAtIndex(
    size_t index) const {
  return nullptr;
}

bool AXPlatformNodeDelegate::HasModalDialog() const {
  return false;
}

gfx::NativeViewAccessible AXPlatformNodeDelegate::GetFirstChild() const {
  if (GetChildCount() > 0)
    return ChildAtIndex(0);
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegate::GetLastChild() const {
  size_t child_count = GetChildCount();
  if (child_count > 0)
    return ChildAtIndex(child_count - 1);
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegate::GetNextSibling() const {
  AXPlatformNodeDelegate* parent = GetParentDelegate();
  if (!parent)
    return nullptr;
  auto index = GetIndexInParent();
  if (index.has_value()) {
    size_t next_index = index.value() + 1;
    if (next_index < parent->GetChildCount())
      return parent->ChildAtIndex(next_index);
  }
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegate::GetPreviousSibling() const {
  AXPlatformNodeDelegate* parent = GetParentDelegate();
  if (!parent)
    return nullptr;
  auto index = GetIndexInParent();
  if (index.has_value()) {
    size_t next_index = index.value() - 1;
    if (next_index < parent->GetChildCount())
      return parent->ChildAtIndex(next_index);
  }
  return nullptr;
}

bool AXPlatformNodeDelegate::IsChildOfLeaf() const {
  if (node_)
    return node_->IsChildOfLeaf();

  // TODO(nektar): Make all tree traversal methods const and remove const_cast.
  const AXPlatformNodeDelegate* parent =
      const_cast<AXPlatformNodeDelegate*>(this)->GetParentDelegate();
  if (!parent)
    return false;
  if (parent->IsLeaf())
    return true;
  return parent->IsChildOfLeaf();
}

bool AXPlatformNodeDelegate::IsDescendantOfAtomicTextField() const {
  if (node_)
    return node_->IsDescendantOfAtomicTextField();

  // TODO(nektar): Add const to all tree traversal methods and remove
  // const_cast.
  for (AXPlatformNodeDelegate* ancestor_delegate =
           const_cast<AXPlatformNodeDelegate*>(this);
       ancestor_delegate;
       ancestor_delegate = static_cast<AXPlatformNodeDelegate*>(
           ancestor_delegate->GetParentDelegate())) {
    if (ancestor_delegate->GetData().IsAtomicTextField())
      return true;
  }
  return false;
}

bool AXPlatformNodeDelegate::IsPlatformDocument() const {
  return ui::IsPlatformDocument(GetRole());
}

bool AXPlatformNodeDelegate::IsFocused() const {
  // TODO(accessibility): Move `GetFocus` into `AXTreeManager` so we can use
  // `BrowserAccessibility` implementation here and remove it from there.
  return false;
}

bool AXPlatformNodeDelegate::IsFocusable() const {
  if (node_)
    return node_->IsFocusable();

  return HasState(ax::mojom::State::kFocusable);
}

bool AXPlatformNodeDelegate::IsIgnored() const {
  if (node_)
    return node_->IsIgnored();

  // To avoid the situation where a screen reader user will not be able to
  // access a focused node because it has accidentally been marked as ignored,
  // we unignore any nodes that are focused. However, we don't need to check
  // this here because subclasses should make sure that the ignored state is
  // removed from all nodes that are currently focused. This condition will be
  // enforced once we switch to using an AXTree of AXNodes in Views.
  return GetRole() == ax::mojom::Role::kNone ||
         HasState(ax::mojom::State::kIgnored);
}

bool AXPlatformNodeDelegate::IsToplevelBrowserWindow() {
  return false;
}

gfx::NativeViewAccessible AXPlatformNodeDelegate::GetLowestPlatformAncestor()
    const {
  AXPlatformNodeDelegate* current_delegate =
      const_cast<AXPlatformNodeDelegate*>(this);
  AXPlatformNodeDelegate* lowest_unignored_delegate = current_delegate;
  if (lowest_unignored_delegate->IsIgnored()) {
    lowest_unignored_delegate = static_cast<AXPlatformNodeDelegate*>(
        lowest_unignored_delegate->GetParentDelegate());
  }
  DCHECK(!lowest_unignored_delegate || !lowest_unignored_delegate->IsIgnored())
      << "`AXPlatformNodeDelegate::GetParentDelegate()` should return "
         "either an unignored object or nullptr.";

  // `highest_leaf_delegate` could be nullptr.
  AXPlatformNodeDelegate* highest_leaf_delegate = lowest_unignored_delegate;
  // For the purposes of this method, a leaf node does not include leaves in the
  // internal accessibility tree, only in the platform exposed tree.
  for (AXPlatformNodeDelegate* ancestor_delegate = lowest_unignored_delegate;
       ancestor_delegate;
       ancestor_delegate = static_cast<AXPlatformNodeDelegate*>(
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

gfx::NativeViewAccessible AXPlatformNodeDelegate::GetTextFieldAncestor() const {
  // TODO(nektar): Add const to all tree traversal methods and remove
  // const_cast.
  for (AXPlatformNodeDelegate* ancestor_delegate =
           const_cast<AXPlatformNodeDelegate*>(this);
       ancestor_delegate;
       ancestor_delegate = static_cast<AXPlatformNodeDelegate*>(
           ancestor_delegate->GetParentDelegate())) {
    if (ancestor_delegate->GetData().IsTextField())
      return ancestor_delegate->GetNativeViewAccessible();
  }
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegate::GetSelectionContainer()
    const {
  // TODO(nektar): Add const to all tree traversal methods and remove
  // const_cast.
  for (AXPlatformNodeDelegate* ancestor_delegate =
           const_cast<AXPlatformNodeDelegate*>(this);
       ancestor_delegate;
       ancestor_delegate = static_cast<AXPlatformNodeDelegate*>(
           ancestor_delegate->GetParentDelegate())) {
    if (IsContainerWithSelectableChildren(ancestor_delegate->GetRole()))
      return ancestor_delegate->GetNativeViewAccessible();
  }
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegate::GetTableAncestor() const {
  // TODO(nektar): Add const to all tree traversal methods and remove
  // const_cast.
  for (AXPlatformNodeDelegate* ancestor_delegate =
           const_cast<AXPlatformNodeDelegate*>(this);
       ancestor_delegate;
       ancestor_delegate = static_cast<AXPlatformNodeDelegate*>(
           ancestor_delegate->GetParentDelegate())) {
    if (IsTableLike(ancestor_delegate->GetRole()))
      return ancestor_delegate->GetNativeViewAccessible();
  }
  return nullptr;
}

std::unique_ptr<ChildIterator> AXPlatformNodeDelegate::ChildrenBegin() const {
  return std::make_unique<ChildIteratorBase>(this, 0);
}

std::unique_ptr<ChildIterator> AXPlatformNodeDelegate::ChildrenEnd() const {
  return std::make_unique<ChildIteratorBase>(this, GetChildCount());
}

const std::string& AXPlatformNodeDelegate::GetName() const {
  if (node_)
    return node()->GetNameUTF8();
  return GetStringAttribute(ax::mojom::StringAttribute::kName);
}

const std::string& AXPlatformNodeDelegate::GetDescription() const {
  return GetStringAttribute(ax::mojom::StringAttribute::kDescription);
}

std::u16string AXPlatformNodeDelegate::GetHypertext() const {
  // Overloaded by platforms which require a hypertext accessibility text
  // implementation.
  return std::u16string();
}

const std::map<int, int>&
AXPlatformNodeDelegate::GetHypertextOffsetToHyperlinkChildIndex() const {
  if (node_)
    return node_->GetHypertextOffsetToHyperlinkChildIndex();

  // TODO(nektar): Remove this method once hypertext computation and
  // selection handling has moved entirely to AXNode / AXPosition.
  static base::NoDestructor<std::map<int, int>> empty_map;
  return *empty_map;
}

bool AXPlatformNodeDelegate::SetHypertextSelection(int start_offset,
                                                   int end_offset) {
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kSetSelection;
  action_data.anchor_node_id = action_data.focus_node_id = GetData().id;
  action_data.anchor_offset = start_offset;
  action_data.focus_offset = end_offset;
  return AccessibilityPerformAction(action_data);
}

TextAttributeMap AXPlatformNodeDelegate::ComputeTextAttributeMap(
    const TextAttributeList& default_attributes) const {
  TextAttributeMap attributes_map;
  attributes_map[0] = default_attributes;
  return attributes_map;
}

std::wstring AXPlatformNodeDelegate::ComputeListItemNameFromContent() const {
  NOTIMPLEMENTED();
  return std::wstring();
}

std::string AXPlatformNodeDelegate::GetInheritedFontFamilyName() const {
  return GetInheritedStringAttribute(ax::mojom::StringAttribute::kFontFamily);
}

gfx::Rect AXPlatformNodeDelegate::GetBoundsRect(
    const AXCoordinateSystem coordinate_system,
    const AXClippingBehavior clipping_behavior,
    AXOffscreenResult* offscreen_result) const {
  return gfx::Rect();
}

gfx::Rect AXPlatformNodeDelegate::GetHypertextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    const AXCoordinateSystem coordinate_system,
    const AXClippingBehavior clipping_behavior,
    AXOffscreenResult* offscreen_result) const {
  return gfx::Rect();
}

gfx::Rect AXPlatformNodeDelegate::GetInnerTextRangeBoundsRect(
    const int start_offset,
    const int end_offset,
    const AXCoordinateSystem coordinate_system,
    const AXClippingBehavior clipping_behavior,
    AXOffscreenResult* offscreen_result) const {
  return gfx::Rect();
}

gfx::NativeViewAccessible AXPlatformNodeDelegate::HitTestSync(
    int screen_physical_pixel_x,
    int screen_physical_pixel_y) const {
  return nullptr;
}

gfx::NativeViewAccessible AXPlatformNodeDelegate::GetFocus() const {
  return nullptr;
}

bool AXPlatformNodeDelegate::IsOffscreen() const {
  return false;
}

bool AXPlatformNodeDelegate::IsMinimized() const {
  return false;
}

bool AXPlatformNodeDelegate::IsText() const {
  if (node_)
    return node_->IsText();
  return ui::IsText(GetRole());
}

bool AXPlatformNodeDelegate::IsWebContent() const {
  return false;
}

bool AXPlatformNodeDelegate::HasVisibleCaretOrSelection() const {
  return IsDescendantOfAtomicTextField();
}

AXPlatformNode* AXPlatformNodeDelegate::GetFromNodeID(int32_t id) {
  return nullptr;
}

AXPlatformNode* AXPlatformNodeDelegate::GetFromTreeIDAndNodeID(
    const AXTreeID& ax_tree_id,
    int32_t id) {
  return nullptr;
}

AXPlatformNode* AXPlatformNodeDelegate::GetTargetNodeForRelation(
    ax::mojom::IntAttribute attr) {
  DCHECK(IsNodeIdIntAttribute(attr));

  if (!HasIntAttribute(attr)) {
    return nullptr;
  }

  AXPlatformNode* node = GetFromNodeID(GetIntAttribute(attr));
  if (!IsValidRelationTarget(node)) {
    return nullptr;
  }

  return node;
}

std::vector<AXPlatformNode*> AXPlatformNodeDelegate::GetTargetNodesForRelation(
    ax::mojom::IntListAttribute attr) {
  DCHECK(IsNodeIdIntListAttribute(attr));
  if (!HasIntListAttribute(attr)) {
    return std::vector<AXPlatformNode*>();
  }

  // If we use std::set to eliminate duplicates, the resulting set will be
  // sorted by the id and we will lose the original order which may be of
  // interest to ATs. The number of ids should be small.

  std::vector<AXPlatformNode*> nodes;
  const std::vector<int32_t>& target_ids = GetIntListAttribute(attr);

  for (int32_t target_id : target_ids) {
    AXPlatformNode* target = GetFromNodeID(target_id);
    if (IsValidRelationTarget(target) && !base::Contains(nodes, target)) {
      nodes.push_back(target);
    }
  }

  return nodes;
}

std::vector<AXPlatformNode*>
AXPlatformNodeDelegate::GetSourceNodesForReverseRelations(
    ax::mojom::IntAttribute attr) {
  // TODO(accessibility) Implement these if views ever use relations more
  // widely. The use so far has been for the Omnibox to the suggestion
  // popup. If this is ever implemented, then the "popup for" to "controlled
  // by" mapping in AXPlatformRelationWin can be removed, as it would be
  // redundant with setting the controls relationship.
  return std::vector<AXPlatformNode*>();
}

std::vector<AXPlatformNode*>
AXPlatformNodeDelegate::GetSourceNodesForReverseRelations(
    ax::mojom::IntListAttribute attr) {
  return std::vector<AXPlatformNode*>();
}

std::vector<AXPlatformNode*> AXPlatformNodeDelegate::GetNodesFromRelationIdSet(
    const std::set<AXNodeID>& ids) {
  std::vector<AXPlatformNode*> nodes;

  for (AXNodeID node_id : ids) {
    AXPlatformNode* node = GetFromNodeID(node_id);
    if (IsValidRelationTarget(node)) {
      nodes.push_back(node);
    }
  }
  return nodes;
}

bool AXPlatformNodeDelegate::IsValidRelationTarget(
    AXPlatformNode* target) const {
  if (!target) {
    // This can occur when the target of the relation was not included in the
    // tree, e.g. it was display:none or role="none".
    // By returning false here, the relation will not be included in the
    // relations reported via platform APIs.
    return false;
  }
  DCHECK_GT(GetUniqueId(), AXPlatformNodeId());
  DCHECK(target);
  DCHECK_GT(target->GetUniqueId(), AXPlatformNodeId());
  // We should ignore reflexive relations.
  return GetUniqueId() != target->GetUniqueId();
}

std::u16string AXPlatformNodeDelegate::GetAuthorUniqueId() const {
  if (node_)
    return node_->GetString16Attribute(ax::mojom::StringAttribute::kHtmlId);
  return std::u16string();
}

AXPlatformNodeId AXPlatformNodeDelegate::GetUniqueId() const {
  static const base::NoDestructor<AXUniqueId> empty_unique_id(
      AXUniqueId::Create());
  return empty_unique_id->Get();
}

AXPlatformNodeDelegate* AXPlatformNodeDelegate::GetParentDelegate() const {
  AXPlatformNode* parent_node =
      AXPlatformNode::FromNativeViewAccessible(GetParent());
  if (parent_node)
    return parent_node->GetDelegate();
  return nullptr;
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
  if (HasBoolAttribute(attribute)) {
    *value = GetBoolAttribute(attribute);
    return true;
  }
  return false;
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
  if (HasFloatAttribute(attribute)) {
    *value = GetFloatAttribute(attribute);
    return true;
  }
  return false;
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
  if (HasIntAttribute(attribute)) {
    *value = GetIntAttribute(attribute);
    return true;
  }
  return false;
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
  bool found = HasStringAttribute(attribute);
  if (found) {
    *value = GetStringAttribute(attribute);
  }
  return found;
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
  bool found = HasStringAttribute(attribute);
  if (found) {
    *value = GetString16Attribute(attribute);
  }
  return found;
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
  bool found = HasIntListAttribute(attribute);
  if (found) {
    *value = GetIntListAttribute(attribute);
  }
  return found;
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
  bool found = HasStringListAttribute(attribute);
  if (found) {
    *value = GetStringListAttribute(attribute);
  }
  return found;
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
  bool found = HasHtmlAttribute(attribute);
  if (found) {
    *value = node_ ? node_->GetHtmlAttribute(attribute)
                   : GetData().GetHtmlAttribute(attribute);
  }
  return found;
}

bool AXPlatformNodeDelegate::GetHtmlAttribute(const char* attribute,
                                              std::u16string* value) const {
  bool found = HasHtmlAttribute(attribute);
  if (found) {
    *value = node_ ? node_->GetHtmlAttributeUTF16(attribute)
                   : GetData().GetHtmlAttributeUTF16(attribute);
  }
  return found;
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
  // TODO(crbug.com/40869533): Do this only if (HasDefaultActionVerb()), After
  // some time tracking the DCHECK at
  // BrowserAccessibilityManager::DoDefaultAction()
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

bool AXPlatformNodeDelegate::IsTable() const {
  if (node_)
    return node_->IsTable();
  return ui::IsTableLike(GetRole());
}

std::optional<int> AXPlatformNodeDelegate::GetTableRowCount() const {
  if (node_)
    return node_->GetTableRowCount();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableRowCount);
}

std::optional<int> AXPlatformNodeDelegate::GetTableColCount() const {
  if (node_)
    return node_->GetTableColCount();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableColumnCount);
}

std::optional<int> AXPlatformNodeDelegate::GetTableCellCount() const {
  if (node_)
    return node_->GetTableCellCount();
  return std::nullopt;
}

std::optional<int> AXPlatformNodeDelegate::GetTableAriaColCount() const {
  if (node_) {
    return node_->GetTableAriaColCount();
  }
  if (!HasIntAttribute(ax::mojom::IntAttribute::kAriaColumnCount)) {
    return std::nullopt;
  }
  return GetIntAttribute(ax::mojom::IntAttribute::kAriaColumnCount);
}

std::optional<int> AXPlatformNodeDelegate::GetTableAriaRowCount() const {
  if (node_) {
    return node_->GetTableAriaRowCount();
  }
  if (!HasIntAttribute(ax::mojom::IntAttribute::kAriaRowCount)) {
    return std::nullopt;
  }
  return GetIntAttribute(ax::mojom::IntAttribute::kAriaRowCount);
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

AXPlatformNode* AXPlatformNodeDelegate::GetTableCaption() const {
  return nullptr;
}

bool AXPlatformNodeDelegate::IsTableRow() const {
  if (node_)
    return node_->IsTableRow();
  return ui::IsTableRow(GetRole());
}

std::optional<int> AXPlatformNodeDelegate::GetTableRowRowIndex() const {
  if (node_)
    return node_->GetTableRowRowIndex();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableRowIndex);
}

bool AXPlatformNodeDelegate::IsTableCellOrHeader() const {
  if (node_)
    return node_->IsTableCellOrHeader();
  return ui::IsCellOrTableHeader(GetRole());
}

std::optional<int> AXPlatformNodeDelegate::GetTableCellIndex() const {
  if (node_)
    return node_->GetTableCellIndex();
  return std::nullopt;
}

std::optional<int> AXPlatformNodeDelegate::GetTableCellColIndex() const {
  if (node_)
    return node_->GetTableCellColIndex();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnIndex);
}

std::optional<int> AXPlatformNodeDelegate::GetTableCellRowIndex() const {
  if (node_)
    return node_->GetTableCellRowIndex();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowIndex);
}

std::optional<int> AXPlatformNodeDelegate::GetTableCellColSpan() const {
  if (node_)
    return node_->GetTableCellColSpan();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellColumnSpan);
}

std::optional<int> AXPlatformNodeDelegate::GetTableCellRowSpan() const {
  if (node_)
    return node_->GetTableCellRowSpan();
  return GetIntAttribute(ax::mojom::IntAttribute::kTableCellRowSpan);
}

std::optional<int> AXPlatformNodeDelegate::GetTableCellAriaColIndex() const {
  if (node_)
    return node_->GetTableCellAriaColIndex();
  if (HasIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnIndex))
    return GetIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnIndex);
  return std::nullopt;
}

std::optional<int> AXPlatformNodeDelegate::GetTableCellAriaRowIndex() const {
  if (node_)
    return node_->GetTableCellAriaRowIndex();
  if (HasIntAttribute(ax::mojom::IntAttribute::kAriaCellRowIndex))
    return GetIntAttribute(ax::mojom::IntAttribute::kAriaCellRowIndex);
  return std::nullopt;
}

std::optional<int32_t> AXPlatformNodeDelegate::GetCellId(int row_index,
                                                         int col_index) const {
  if (node_) {
    AXNode* cell = node()->GetTableCellFromCoords(row_index, col_index);
    if (!cell)
      return std::nullopt;
    return cell->id();
  }
  return std::nullopt;
}

std::optional<int32_t> AXPlatformNodeDelegate::GetCellIdAriaCoords(
    int aria_row_index,
    int aria_col_index) const {
  if (node_) {
    AXNode* cell =
        node()->GetTableCellFromAriaCoords(aria_row_index, aria_col_index);
    if (!cell) {
      return std::nullopt;
    }
    return cell->id();
  }
  return std::nullopt;
}

std::optional<int32_t> AXPlatformNodeDelegate::CellIndexToId(
    int cell_index) const {
  if (node_) {
    AXNode* cell = node()->GetTableCellFromIndex(cell_index);
    if (!cell)
      return std::nullopt;
    return cell->id();
  }
  return std::nullopt;
}

bool AXPlatformNodeDelegate::IsCellOrHeaderOfAriaGrid() const {
  if (node_)
    return node_->IsCellOrHeaderOfAriaGrid();
  return false;
}

bool AXPlatformNodeDelegate::IsRootWebAreaForPresentationalIframe() const {
  if (node_)
    return node_->IsRootWebAreaForPresentationalIframe();
  if (!ui::IsPlatformDocument(GetRole()))
    return false;
  AXPlatformNodeDelegate* parent = GetParentDelegate();
  if (!parent)
    return false;
  return parent->GetRole() == ax::mojom::Role::kIframePresentational;
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

std::optional<int> AXPlatformNodeDelegate::GetPosInSet() const {
  return std::nullopt;
}

std::optional<int> AXPlatformNodeDelegate::GetSetSize() const {
  return std::nullopt;
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

gfx::AcceleratedWidget
AXPlatformNodeDelegate::GetTargetForNativeAccessibilityEvent() {
  return gfx::kNullAcceleratedWidget;
}

bool AXPlatformNodeDelegate::AccessibilityPerformAction(
    const AXActionData& data) {
  return false;
}

std::u16string
AXPlatformNodeDelegate::GetLocalizedRoleDescriptionForUnlabeledImage() const {
  return std::u16string();
}

std::u16string
AXPlatformNodeDelegate::GetLocalizedStringForImageAnnotationStatus(
    ax::mojom::ImageAnnotationStatus status) const {
  return std::u16string();
}

std::u16string AXPlatformNodeDelegate::GetLocalizedStringForLandmarkType()
    const {
  return std::u16string();
}

std::u16string AXPlatformNodeDelegate::GetLocalizedStringForRoleDescription()
    const {
  return std::u16string();
}

std::u16string AXPlatformNodeDelegate::GetStyleNameAttributeAsLocalizedString()
    const {
  return std::u16string();
}

void AXPlatformNodeDelegate::SetIsPrimaryWebContentsForWindow() {}

bool AXPlatformNodeDelegate::IsPrimaryWebContentsForWindow() const {
  return false;
}

bool AXPlatformNodeDelegate::ShouldIgnoreHoveredStateForTesting() {
  return true;
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

bool AXPlatformNodeDelegate::IsIA2NodeSelected() const {
  if (node_) {
    return node_->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
  }
  return false;
}

bool AXPlatformNodeDelegate::IsUIANodeSelected() const {
  if (node_) {
    // https://www.w3.org/TR/core-aam-1.1/#mapping_state-property_table
    // SelectionItem.IsSelected is set according to the True or False value of
    // aria-checked for 'radio' and 'menuitemradio' roles.
    if (ui::IsRadio(node_->GetRole())) {
      return GetData().GetCheckedState() == ax::mojom::CheckedState::kTrue;
    }

    return GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
  }

  return false;
}

const std::vector<gfx::NativeViewAccessible>
AXPlatformNodeDelegate::GetUIADirectChildrenInRange(
    AXPlatformNodeDelegate* start,
    AXPlatformNodeDelegate* end) {
  return {};
}

std::string AXPlatformNodeDelegate::GetLanguage() const {
  if (node_)
    return node_->GetLanguage();
  return std::string();
}

std::string AXPlatformNodeDelegate::SubtreeToStringHelper(size_t level) {
  std::string result(level * 2, '+');
  result += ToString();
  result += '\n';

  // We can't use ChildrenBegin() and ChildrenEnd() here, because they both
  // return an std::unique_ptr<ChildIterator> which is an abstract class.
  //
  // TODO(accessibility): CHildrenBegin and ChildrenEnd can now be used, use
  // here.
  auto iter_start = ChildIteratorBase(this, 0);
  auto iter_end = ChildIteratorBase(this, GetChildCount());
  for (auto iter = iter_start; iter != iter_end; ++iter) {
    AXPlatformNodeDelegate& child = static_cast<AXPlatformNodeDelegate&>(*iter);
    result += child.SubtreeToStringHelper(level + 1);
  }

  return result;
}

}  // namespace ui
