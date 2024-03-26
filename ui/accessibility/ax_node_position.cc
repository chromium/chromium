// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_position.h"

#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/buildflags.h"

namespace ui {

// On some platforms, most objects are represented in the text of their parents
// with a special "embedded object character" and not with their actual text
// contents. Also on the same platforms, if a node has only ignored descendants,
// i.e., it appears to be empty to assistive software, we need to treat it as a
// character and a word boundary.
AXEmbeddedObjectBehavior g_ax_embedded_object_behavior =
#if BUILDFLAG(IS_WIN) || BUILDFLAG(USE_ATK)
    AXEmbeddedObjectBehavior::kExposeCharacterForHypertext;
#else
    AXEmbeddedObjectBehavior::kSuppressCharacter;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(USE_ATK)

ScopedAXEmbeddedObjectBehaviorSetter::ScopedAXEmbeddedObjectBehaviorSetter(
    AXEmbeddedObjectBehavior behavior) {
  prev_behavior_ = g_ax_embedded_object_behavior;
  g_ax_embedded_object_behavior = behavior;
}

ScopedAXEmbeddedObjectBehaviorSetter::~ScopedAXEmbeddedObjectBehaviorSetter() {
  g_ax_embedded_object_behavior = prev_behavior_;
}

std::string ToString(const AXPositionKind kind) {
  static constexpr auto kKindToString =
      base::MakeFixedFlatMap<AXPositionKind, const char*>(
          {{AXPositionKind::NULL_POSITION, "NullPosition"},
           {AXPositionKind::TREE_POSITION, "TreePosition"},
           {AXPositionKind::TEXT_POSITION, "TextPosition"}});

  const auto iter = kKindToString.find(kind);
  if (iter == std::end(kKindToString))
    return std::string();
  return iter->second;
}

// static
AXNodePosition::AXPositionInstance AXNodePosition::CreatePosition(
    const AXNode& node,
    int child_index_or_text_offset,
    ax::mojom::TextAffinity affinity) {
  if (!node.tree())
    return CreateNullPosition();

  if (IsTextPositionAnchor(node)) {
    // TODO(accessibility) It is a mistake for the to caller try to create a
    // text position with BEFORE_TEXT as the text offset. Correct the callers
    // that are doing this.
    // DCHECK_NE(child_index_or_text_offset, BEFORE_TEXT)
    // << "Creating a text position with BEFORE_TEXT as the offset is illegal "
    //    "and disallowed.";
    int text_offset = child_index_or_text_offset == BEFORE_TEXT
                          ? 0
                          : child_index_or_text_offset;
    return CreateTextPosition(node, text_offset, affinity);
  }

  DCHECK_LE(child_index_or_text_offset,
            static_cast<int>(node.GetChildCountCrossingTreeBoundary()))
      << "\n* Trying to create a tree position with a child index that is too "
         "large. Maybe a text position should have been created instead?\n"
      << "\n* Anchor node: " << node << "\n* IsLeaf(): " << node.IsLeaf()
      << "\n* Child offset: " << child_index_or_text_offset
      << "\n* IsLeafNodeForTreePosition(): " << IsLeafNodeForTreePosition(node)
      << "\n* Tree: " << node.tree()->ToString();

  return CreateTreePosition(node, child_index_or_text_offset);
}

// static
bool AXNodePosition::IsTextPositionAnchor(const AXNode& node) {
  // TODO(accessibility) Simplify. Not actually sure if this is the correct
  // thing for the case where IsLeaf() == false but IsLeafNodeForTreePosition()
  // is true.
  if (node.IsLeaf())
    return true;

  // TODO(accessibility) Try to remove this condition. Text positions for a
  // selection operation should only be created inside selectable text.
  // A list marker for example is not selectable text: it would either be
  // selected as a whole or not selected, and you can't select half of it.
  if (IsLeafNodeForTreePosition(node))
    return true;

  if (node.GetRole() == ax::mojom::Role::kSpinButton) {
    // TODO(benjamin.beaudry) Please look into whether this code needs to
    // remain, or can be simplified.
    return true;
  }

  // Ignored atomic text fields and spin buttons are not considered leaves by
  // AXNode::IsLeaf(), but should always use a text position.
  if (node.data().IsAtomicTextField()) {
    // Ignored atomic text fields and spin buttons are not considered leaves by
    // AXNode::IsLeaf(), but should always use a text position.
    // TODO(accessibility) Nobody should be creating a text position on an
    // ignored text field.
    DCHECK(node.IsIgnored()) << "Returned false from IsLeaf(): " << node;
    return true;
  }

  return false;
}

AXNodePosition::AXNodePosition() = default;

AXNodePosition::~AXNodePosition() = default;

AXNodePosition::AXNodePosition(const AXNodePosition& other) = default;

AXNodePosition::AXPositionInstance AXNodePosition::Clone() const {
  return AXPositionInstance(new AXNodePosition(*this));
}

}  // namespace ui
