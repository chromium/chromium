// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_position.h"

#include "base/strings/string_util.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

AXTree* AXNodePosition::tree_ = nullptr;

AXNodePosition::AXNodePosition() {}

AXNodePosition::~AXNodePosition() {}

AXNodePosition::AXPositionInstance AXNodePosition::Clone() const {
  return AXPositionInstance(new AXNodePosition(*this));
}

base::string16 AXNodePosition::GetInnerText() const {
  if (IsNullPosition())
    return base::string16();

  DCHECK(GetAnchor());
  base::string16 value = GetAnchor()->data().GetString16Attribute(
      ax::mojom::StringAttribute::kValue);
  if (!value.empty())
    return value;
  return GetAnchor()->data().GetString16Attribute(
      ax::mojom::StringAttribute::kName);
}

void AXNodePosition::AnchorChild(int child_index,
                                 AXTreeID* tree_id,
                                 int32_t* child_id) const {
  DCHECK(tree_id);
  DCHECK(child_id);

  if (!GetAnchor() || child_index < 0 || child_index >= AnchorChildCount()) {
    *tree_id = AXTreeIDUnknown();
    *child_id = INVALID_ANCHOR_ID;
    return;
  }

  AXNode* child = GetAnchor()->ChildAtIndex(child_index);
  DCHECK(child);
  *tree_id = this->tree_id();
  *child_id = child->id();
}

int AXNodePosition::AnchorChildCount() const {
  return GetAnchor() ? GetAnchor()->child_count() : 0;
}

int AXNodePosition::AnchorIndexInParent() const {
  return GetAnchor() ? GetAnchor()->index_in_parent() : INVALID_INDEX;
}

void AXNodePosition::AnchorParent(AXTreeID* tree_id, int32_t* parent_id) const {
  DCHECK(tree_id);
  DCHECK(parent_id);

  if (!GetAnchor() || !GetAnchor()->parent()) {
    *tree_id = AXTreeIDUnknown();
    *parent_id = INVALID_ANCHOR_ID;
    return;
  }

  AXNode* parent = GetAnchor()->parent();
  *tree_id = this->tree_id();
  *parent_id = parent->id();
}

AXNode* AXNodePosition::GetNodeInTree(AXTreeID tree_id, int32_t node_id) const {
  if (!tree_ || node_id == INVALID_ANCHOR_ID)
    return nullptr;
  return AXNodePosition::tree_->GetFromId(node_id);
}

int AXNodePosition::MaxTextOffset() const {
  if (IsNullPosition())
    return INVALID_INDEX;
  return static_cast<int>(GetInnerText().length());
}

bool AXNodePosition::IsInWhiteSpace() const {
  switch (kind()) {
    case AXPositionKind::NULL_POSITION:
      return false;
    case AXPositionKind::TREE_POSITION:
    case AXPositionKind::TEXT_POSITION:
      return base::ContainsOnlyChars(GetInnerText(), base::kWhitespaceUTF16);
  }
  NOTREACHED();
  return false;
}

std::vector<int32_t> AXNodePosition::GetWordStartOffsets() const {
  if (IsNullPosition())
    return std::vector<int32_t>();
  DCHECK(GetAnchor());
  return GetAnchor()->data().GetIntListAttribute(
      ax::mojom::IntListAttribute::kWordStarts);
}

std::vector<int32_t> AXNodePosition::GetWordEndOffsets() const {
  if (IsNullPosition())
    return std::vector<int32_t>();
  DCHECK(GetAnchor());
  return GetAnchor()->data().GetIntListAttribute(
      ax::mojom::IntListAttribute::kWordEnds);
}

int32_t AXNodePosition::GetNextOnLineID(int32_t node_id) const {
  if (IsNullPosition())
    return INVALID_ANCHOR_ID;
  AXNode* node = GetNodeInTree(tree_id(), node_id);
  int next_on_line_id;
  if (!node || !node->data().GetIntAttribute(
                   ax::mojom::IntAttribute::kNextOnLineId, &next_on_line_id)) {
    return INVALID_ANCHOR_ID;
  }
  return static_cast<int32_t>(next_on_line_id);
}

int32_t AXNodePosition::GetPreviousOnLineID(int32_t node_id) const {
  if (IsNullPosition())
    return INVALID_ANCHOR_ID;
  AXNode* node = GetNodeInTree(tree_id(), node_id);
  int previous_on_line_id;
  if (!node ||
      !node->data().GetIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                                    &previous_on_line_id)) {
    return INVALID_ANCHOR_ID;
  }
  return static_cast<int32_t>(previous_on_line_id);
}

}  // namespace ui
