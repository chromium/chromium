// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_delegate_base.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_constants.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"

namespace ui {

AXPlatformNodeDelegateBase::AXPlatformNodeDelegateBase() = default;

AXPlatformNodeDelegateBase::~AXPlatformNodeDelegateBase() = default;

AXPlatformNodeDelegateBase::ChildIteratorBase::ChildIteratorBase(
    AXPlatformNodeDelegateBase* parent,
    size_t index)
    : index_(index), parent_(parent) {
  DCHECK(parent);
  DCHECK(index <= parent->GetChildCount());
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
  DCHECK_GT(index_, 0u);
  index_--;
  return *this;
}

AXPlatformNodeDelegateBase::ChildIteratorBase&
AXPlatformNodeDelegateBase::ChildIteratorBase::operator--(int) {
  DCHECK_GT(index_, 0u);
  index_--;
  return *this;
}

gfx::NativeViewAccessible
AXPlatformNodeDelegateBase::ChildIteratorBase::GetNativeViewAccessible() const {
  if (index_ < parent_->GetChildCount())
    return parent_->ChildAtIndex(index_);

  return nullptr;
}

absl::optional<size_t>
AXPlatformNodeDelegateBase::ChildIteratorBase::GetIndexInParent() const {
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

AXPlatformNode* AXPlatformNodeDelegateBase::GetFromNodeID(int32_t id) {
  return nullptr;
}

AXPlatformNode* AXPlatformNodeDelegateBase::GetFromTreeIDAndNodeID(
    const ui::AXTreeID& ax_tree_id,
    int32_t id) {
  return nullptr;
}

gfx::AcceleratedWidget
AXPlatformNodeDelegateBase::GetTargetForNativeAccessibilityEvent() {
  return gfx::kNullAcceleratedWidget;
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

AXPlatformNode* AXPlatformNodeDelegateBase::GetTableCaption() const {
  return nullptr;
}

bool AXPlatformNodeDelegateBase::IsRootWebAreaForPresentationalIframe() const {
  if (!ui::IsPlatformDocument(GetRole()))
    return false;
  AXPlatformNodeDelegate* parent = GetParentDelegate();
  if (!parent)
    return false;
  return parent->GetRole() == ax::mojom::Role::kIframePresentational;
}

absl::optional<int> AXPlatformNodeDelegateBase::GetPosInSet() const {
  return absl::nullopt;
}

absl::optional<int> AXPlatformNodeDelegateBase::GetSetSize() const {
  return absl::nullopt;
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

bool AXPlatformNodeDelegateBase::ShouldIgnoreHoveredStateForTesting() {
  return true;
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
      if (!base::Contains(nodes, node))
        nodes.push_back(node);
    }
  }

  return nodes;
}

const std::vector<gfx::NativeViewAccessible>
AXPlatformNodeDelegateBase::GetUIADirectChildrenInRange(
    ui::AXPlatformNodeDelegate* start,
    ui::AXPlatformNodeDelegate* end) {
  return {};
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
