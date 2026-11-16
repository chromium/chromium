// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node_position.h"

#include "base/containers/fixed_flat_map.h"
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
    ax::mojom::TextAffinity affinity,
    std::optional<ax::mojom::OffsetType> offset_type) {
  if (!node.tree()) {
    return CreateNullPosition();
  }

  // TODO(crbug.com/527726308): Consider adding an Android-only CHECK that
  // `offset_type` is not `std::nullopt` once all Android callers provide it.
  const ax::mojom::OffsetType resolved_offset_type = offset_type.value_or(
      IsTextPositionAnchor(node) ? ax::mojom::OffsetType::kText
                                 : ax::mojom::OffsetType::kChild);

  if (resolved_offset_type == ax::mojom::OffsetType::kText) {
    // TODO(accessibility) It is a mistake for the caller to try to create a
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

  const int child_count =
      static_cast<int>(node.GetChildCountCrossingTreeBoundary());
  DCHECK_LE(child_index_or_text_offset, child_count)
      << "\n* Trying to create a tree position with a child index that is too "
         "large. Maybe a text position should have been created instead?\n"
      << "\n* Anchor node: " << node << "\n* IsLeaf(): " << node.IsLeaf()
      << "\n* Child offset: " << child_index_or_text_offset
      << "\n* IsLeafNodeForTreePosition(): " << IsLeafNodeForTreePosition(node)
      << "\n* Tree: " << node.tree()->ToString();

  // `IsTextPositionAnchor(node)` returns `true` whenever
  // `IsLeafNodeForTreePosition(node)` is `true`, so callers omitting
  // `offset_type` (`std::nullopt`) always resolve to `OffsetType::kText` above
  // and are unaffected. This branch is only reached when the caller explicitly
  // passes `OffsetType::kChild`. On leaf nodes, `AXPosition::IsValid()`
  // requires a tree position's `child_index()` to be either `BEFORE_TEXT`
  // (start of anchor) or `AnchorChildCount()` (end of anchor, when
  // `child_count > 0`).
  if (IsLeafNodeForTreePosition(node)) {
    CHECK(offset_type == ax::mojom::OffsetType::kChild);
    if (child_count > 0 && child_index_or_text_offset >= child_count) {
      return CreateTreePositionAtEndOfAnchor(node);
    }
    return CreateTreePositionAtStartOfAnchor(node);
  }

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
    // TODO(crbug.com/581458342) Please look into whether this code needs to
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
