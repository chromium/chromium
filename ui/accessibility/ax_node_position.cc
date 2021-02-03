// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_position.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/base/buildflags.h"

namespace ui {

// On some platforms, most objects are represented in the text of their parents
// with a special "embedded object character" and not with their actual text
// contents. Also on the same platforms, if a node has only ignored descendants,
// i.e., it appears to be empty to assistive software, we need to treat it as a
// character and a word boundary.
AXEmbeddedObjectBehavior g_ax_embedded_object_behavior =
#if defined(OS_WIN) || BUILDFLAG(USE_ATK)
    AXEmbeddedObjectBehavior::kExposeCharacter;
#else
    AXEmbeddedObjectBehavior::kSuppressCharacter;
#endif  // defined(OS_WIN) || BUILDFLAG(USE_ATK)

// static
AXNodePosition::AXPositionInstance AXNodePosition::CreatePosition(
    const AXNode& node,
    int child_index_or_text_offset,
    ax::mojom::TextAffinity affinity) {
  if (!node.tree())
    return CreateNullPosition();

  AXTreeID tree_id = node.tree()->GetAXTreeID();
  if (node.IsLeaf()) {
    return CreateTextPosition(tree_id, node.id(), child_index_or_text_offset,
                              affinity);
  }

  return CreateTreePosition(tree_id, node.id(), child_index_or_text_offset);
}

AXNodePosition::AXNodePosition() = default;

AXNodePosition::~AXNodePosition() = default;

AXNodePosition::AXNodePosition(const AXNodePosition& other)
    : AXPosition<AXNodePosition, AXNode>(other) {}

AXNodePosition::AXPositionInstance AXNodePosition::Clone() const {
  return AXPositionInstance(new AXNodePosition(*this));
}

base::string16 AXNodePosition::GetText() const {
  if (IsNullPosition())
    return base::string16();

  // Special case, if a position's anchor node has only ignored descendants,
  // i.e., it appears to be empty to assistive software, on some platforms we
  // need to still treat it as a character and a word boundary. We achieve this
  // by adding an embedded object character in the text representation used by
  // this class, but we don't expose that character to assistive software that
  // tries to retrieve the node's inner text.
  if (IsEmptyObjectReplacedByCharacter())
    return AXNode::kEmbeddedCharacter;

  // Special case, if a position's anchor node is hosting another accessibility
  // tree, return the text that is found in that tree's root.
  const AXNode* anchor = GetAnchor();
  const AXTreeManager* child_tree_manager =
      AXTreeManagerMap::GetInstance().GetManagerForChildTree(*anchor);
  if (child_tree_manager) {
    // The child node exists in a separate tree from its parent.
    anchor = child_tree_manager->GetRootAsAXNode();
  }

  switch (g_ax_embedded_object_behavior) {
    case AXEmbeddedObjectBehavior::kSuppressCharacter:
      return base::UTF8ToUTF16(anchor->GetInnerText());
    case AXEmbeddedObjectBehavior::kExposeCharacter:
      return anchor->GetHypertext();
  }
}

bool AXNodePosition::IsInLineBreak() const {
  if (IsNullPosition())
    return false;
  return GetAnchor()->IsLineBreak();
}

bool AXNodePosition::IsInTextObject() const {
  if (IsNullPosition())
    return false;
  return GetAnchor()->IsText();
}

bool AXNodePosition::IsInWhiteSpace() const {
  if (IsNullPosition())
    return false;
  return GetAnchor()->IsLineBreak() ||
         base::ContainsOnlyChars(GetText(), base::kWhitespaceUTF16);
}

int AXNodePosition::MaxTextOffset() const {
  if (IsNullPosition())
    return INVALID_OFFSET;

  // Special case: If a node has only ignored descendants, i.e., it appears to
  // be empty to assistive software, on some platforms we need to still treat it
  // as a character and a word boundary. We achieve this by adding an "object
  // replacement character" in the accessibility tree's text representation, but
  // we don't expose that character to assistive software that tries to retrieve
  // the node's inner text or hypertext.
  if (IsEmptyObjectReplacedByCharacter())
    return AXNode::kEmbeddedCharacterLength;

  // Special case, if a position's anchor node is hosting another accessibility
  // tree, return the text that is found in that tree's root.
  const AXNode* anchor = GetAnchor();
  const AXTreeManager* child_tree_manager =
      AXTreeManagerMap::GetInstance().GetManagerForChildTree(*anchor);
  if (child_tree_manager) {
    // The child node exists in a separate tree from its parent.
    anchor = child_tree_manager->GetRootAsAXNode();
  }

  switch (g_ax_embedded_object_behavior) {
    case AXEmbeddedObjectBehavior::kSuppressCharacter:
      // TODO(nektar): Switch to anchor->GetInnerTextLength() after AXPosition
      // switches to using UTF8.
      return int{base::UTF8ToUTF16(anchor->GetInnerText()).length()};
    case AXEmbeddedObjectBehavior::kExposeCharacter:
      return int{anchor->GetHypertext().length()};
  }
}

ax::mojom::Role AXNodePosition::GetRole() const {
  if (IsNullPosition())
    return ax::mojom::Role::kNone;
  return GetAnchor()->data().role;
}

void AXNodePosition::AnchorChild(int child_index,
                                 AXTreeID* tree_id,
                                 AXNodeID* child_id) const {
  DCHECK(tree_id);
  DCHECK(child_id);
  if (!GetAnchor() || child_index < 0 || child_index >= AnchorChildCount()) {
    *tree_id = AXTreeIDUnknown();
    *child_id = kInvalidAXNodeID;
    return;
  }

  AXNode* child = nullptr;
  const AXTreeManager* child_tree_manager =
      AXTreeManagerMap::GetInstance().GetManagerForChildTree(*GetAnchor());
  if (child_tree_manager) {
    // The child node exists in a separate tree from its parent.
    child = child_tree_manager->GetRootAsAXNode();
    *tree_id = child_tree_manager->GetTreeID();
  } else {
    child = GetAnchor()->children()[size_t{child_index}];
    *tree_id = this->tree_id();
  }
  *child_id = child->id();
}

int AXNodePosition::AnchorChildCount() const {
  if (!GetAnchor())
    return 0;

  const AXTreeManager* child_tree_manager =
      AXTreeManagerMap::GetInstance().GetManagerForChildTree(*GetAnchor());
  if (child_tree_manager)
    return 1;

  return int{GetAnchor()->children().size()};
}

int AXNodePosition::AnchorUnignoredChildCount() const {
  if (!GetAnchor())
    return 0;

  const AXTreeManager* child_tree_manager =
      AXTreeManagerMap::GetInstance().GetManagerForChildTree(*GetAnchor());
  if (child_tree_manager) {
    DCHECK_EQ(GetAnchor()->GetUnignoredChildCount(), 0u)
        << "A node cannot be hosting both a child tree and other nodes as "
           "children.";
    return 1;  // A child tree is never ignored.
  }

  return int{GetAnchor()->GetUnignoredChildCount()};
}

int AXNodePosition::AnchorIndexInParent() const {
  // If this is the root tree, the index in parent will be 0.
  return GetAnchor() ? int{GetAnchor()->index_in_parent()} : INVALID_INDEX;
}

base::stack<AXNode*> AXNodePosition::GetAncestorAnchors() const {
  if (!GetAnchor())
    return base::stack<AXNode*>();

  base::stack<AXNode*> anchors;
  AXNode* current_anchor = GetAnchor();
  AXNodeID current_anchor_id = GetAnchor()->id();
  AXTreeID current_tree_id = tree_id();
  AXNodeID parent_anchor_id = kInvalidAXNodeID;
  AXTreeID parent_tree_id = AXTreeIDUnknown();

  while (current_anchor) {
    anchors.push(current_anchor);
    current_anchor = GetParent(
        current_anchor /*child*/, current_tree_id /*child_tree_id*/,
        &parent_tree_id /*parent_tree_id*/, &parent_anchor_id /*parent_id*/);

    current_anchor_id = parent_anchor_id;
    current_tree_id = parent_tree_id;
  }
  return anchors;
}

AXNode* AXNodePosition::GetLowestUnignoredAncestor() const {
  if (!GetAnchor())
    return nullptr;
  // TODO(nektar): Switch to using GetAnchor()->GetLowestPlatformAncestor().
  return GetAnchor()->GetUnignoredParent();
}

void AXNodePosition::AnchorParent(AXTreeID* tree_id,
                                  AXNodeID* parent_id) const {
  DCHECK(tree_id);
  DCHECK(parent_id);
  *tree_id = AXTreeIDUnknown();
  *parent_id = kInvalidAXNodeID;
  if (!GetAnchor())
    return;

  GetParent(GetAnchor() /*child*/, this->tree_id() /*child_tree_id*/,
            tree_id /*parent_tree_id*/, parent_id /*parent_id*/);
}

AXNode* AXNodePosition::GetNodeInTree(AXTreeID tree_id,
                                      AXNodeID node_id) const {
  if (node_id == kInvalidAXNodeID)
    return nullptr;

  AXTreeManager* manager = AXTreeManagerMap::GetInstance().GetManager(tree_id);
  if (manager)
    return manager->GetNodeFromTree(tree_id, node_id);

  return nullptr;
}

AXNodeID AXNodePosition::GetAnchorID(AXNode* node) const {
  return node->id();
}

AXTreeID AXNodePosition::GetTreeID(AXNode* node) const {
  return node->tree()->GetAXTreeID();
}

bool AXNodePosition::IsEmbeddedObjectInParent() const {
  switch (g_ax_embedded_object_behavior) {
    case AXEmbeddedObjectBehavior::kSuppressCharacter:
      return false;
    case AXEmbeddedObjectBehavior::kExposeCharacter:
      // We expose an "object replacement character" for all nodes except
      // (A) textual nodes and (B) nodes that are invisible to platform APIs,
      // AKA nodes that are descendants of platform leaves. In the former case,
      // textual nodes are represented by their actual text in the text of their
      // parent nodes, in order to maintain compatibility with how Firefox
      // exposes text in IAccessibleText. For the latter case, an example of a
      // platform leaf is a plain text field because all of the accessibility
      // subtree inside the text field is not visible to platform APIs.
      //
      // Please note that for navigational purposes, we need to expose an
      // "object replacement character" in empty controls, such as in an empty
      // text field. The presence or the absence of accessible content inside a
      // control might alter whether an "object replacement character" would be
      // exposed in that control, in contrast to ordinary text such as in the
      // case of a non-empty plain text field which should only have textual
      // nodes inside it. This is because empty controls need to act as a word
      // and character boundary. See
      // `AXPosition::IsEmptyObjectReplacedByCharacter()` for more information.
      return !IsNullPosition() && !GetAnchor()->IsText() &&
             !GetAnchor()->IsChildOfLeaf();
  }
}

bool AXNodePosition::IsInLineBreakingObject() const {
  if (IsNullPosition())
    return false;
  return GetAnchor()->data().GetBoolAttribute(
             ax::mojom::BoolAttribute::kIsLineBreakingObject) &&
         !GetAnchor()->IsInListMarker();
}

ax::mojom::Role AXNodePosition::GetAnchorRole() const {
  if (IsNullPosition())
    return ax::mojom::Role::kNone;
  return GetRole(GetAnchor());
}

ax::mojom::Role AXNodePosition::GetRole(AXNode* node) const {
  return node->data().role;
}

AXNodeTextStyles AXNodePosition::GetTextStyles() const {
  // Check either the current anchor or its parent for text styles.
  AXNodeTextStyles current_anchor_text_styles =
      !IsNullPosition() ? GetAnchor()->data().GetTextStyles()
                        : AXNodeTextStyles();
  if (current_anchor_text_styles.IsUnset()) {
    AXPositionInstance parent_position = AsTreePosition()->CreateParentPosition(
        ax::mojom::MoveDirection::kBackward);
    if (!parent_position->IsNullPosition())
      return parent_position->GetAnchor()->data().GetTextStyles();
  }
  return current_anchor_text_styles;
}

std::vector<int32_t> AXNodePosition::GetWordStartOffsets() const {
  if (IsNullPosition())
    return std::vector<int32_t>();
  DCHECK(GetAnchor());

  // Embedded object replacement characters are not represented in the
  // "kWordStarts" attribute so we need to special case them here.
  if (IsEmptyObjectReplacedByCharacter())
    return {0};

  return GetAnchor()->data().GetIntListAttribute(
      ax::mojom::IntListAttribute::kWordStarts);
}

std::vector<int32_t> AXNodePosition::GetWordEndOffsets() const {
  if (IsNullPosition())
    return std::vector<int32_t>();
  DCHECK(GetAnchor());

  // Embedded object replacement characters are not represented in the
  // "kWordEnds" attribute so we need to special case them here.
  //
  // Since the whole text exposed inside of an embedded object is of
  // length 1 (the embedded object replacement character), the word end offset
  // is positioned at 1. Because we want to treat the embedded object
  // replacement characters as ordinary characters, it wouldn't be consistent to
  // assume they have no length and return 0 instead of 1.
  if (IsEmptyObjectReplacedByCharacter())
    return {1};

  return GetAnchor()->data().GetIntListAttribute(
      ax::mojom::IntListAttribute::kWordEnds);
}

AXNodeID AXNodePosition::GetNextOnLineID(AXNodeID node_id) const {
  if (IsNullPosition())
    return kInvalidAXNodeID;
  AXNode* node = GetNodeInTree(tree_id(), node_id);
  int next_on_line_id;
  if (!node || !node->data().GetIntAttribute(
                   ax::mojom::IntAttribute::kNextOnLineId, &next_on_line_id)) {
    return kInvalidAXNodeID;
  }
  return static_cast<AXNodeID>(next_on_line_id);
}

AXNodeID AXNodePosition::GetPreviousOnLineID(AXNodeID node_id) const {
  if (IsNullPosition())
    return kInvalidAXNodeID;
  AXNode* node = GetNodeInTree(tree_id(), node_id);
  int previous_on_line_id;
  if (!node ||
      !node->data().GetIntAttribute(ax::mojom::IntAttribute::kPreviousOnLineId,
                                    &previous_on_line_id)) {
    return kInvalidAXNodeID;
  }
  return static_cast<AXNodeID>(previous_on_line_id);
}

// static
AXNode* AXNodePosition::GetParent(AXNode* child,
                                  AXTreeID child_tree_id,
                                  AXTreeID* parent_tree_id,
                                  AXNodeID* parent_id) {
  DCHECK(parent_tree_id);
  DCHECK(parent_id);
  *parent_tree_id = AXTreeIDUnknown();
  *parent_id = kInvalidAXNodeID;
  if (!child)
    return nullptr;

  AXNode* parent = child->parent();
  *parent_tree_id = child_tree_id;

  if (!parent) {
    AXTreeManager* manager =
        AXTreeManagerMap::GetInstance().GetManager(child_tree_id);
    if (manager) {
      parent = manager->GetParentNodeFromParentTreeAsAXNode();
      *parent_tree_id = manager->GetParentTreeID();
    }
  }

  if (!parent) {
    *parent_tree_id = AXTreeIDUnknown();
    return parent;
  }

  *parent_id = parent->id();
  return parent;
}

}  // namespace ui
