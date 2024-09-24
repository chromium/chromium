// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_COMPUTED_NODE_DATA_H_
#define UI_ACCESSIBILITY_AX_COMPUTED_NODE_DATA_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_id_forward.h"

namespace ui {

class AXNode;

// Computes and stores information about an `AXNode` that is slow or error-prone
// to compute in the tree's source, e.g. in Blink. This class holds cached
// values that should be re-computed when the associated `AXNode` is in any way
// modified.
class AX_EXPORT AXComputedNodeData final {
 public:
  explicit AXComputedNodeData(const AXNode& node);
  virtual ~AXComputedNodeData();
  AXComputedNodeData(const AXComputedNodeData& other) = delete;
  AXComputedNodeData& operator=(const AXComputedNodeData& other) = delete;

  // If the associated node is unignored, i.e. exposed to the platform's
  // assistive software, its position in the list of unignored children of its
  // lowest unignored parent. Naturally, this value is not defined when the
  // associated node is ignored.
  int GetOrComputeUnignoredIndexInParent() const;

  // The lowest unignored parent. This value should be computed for all
  // associated nodes, ignored and unignored. Only the rootnode should not have
  // an unignored parent.
  AXNodeID GetOrComputeUnignoredParentID() const;
  AXNode* GetOrComputeUnignoredParent() const;

  // If the associated node is unignored, i.e. exposed to the platform's
  // assistive software, the number of its children that are also unignored.
  // Naturally, this value is not defined when the associated node is ignored.
  int GetOrComputeUnignoredChildCount() const;

  // If the associated node is unignored, i.e. exposed to the platform's
  // assistive software, the IDs of its children that are also unignored.
  const std::vector<AXNodeID>& GetOrComputeUnignoredChildIDs() const;

  // Whether the associated node is a descendant of a platform leaf. The set of
  // platform leaves are the lowest nodes that are exposed to the platform's
  // assistive software.
  bool GetOrComputeIsDescendantOfPlatformLeaf() const;

  // Given an accessibility attribute, returns the attribute's value. The
  // attribute is computed if not provided by the tree's source, otherwise it is
  // simply returned from the node's data. String and intlist attributes are
  // potentially the slowest to compute at the tree's source, e.g. in Blink.
  const std::string& ComputeAttributeUTF8(
      const ax::mojom::StringAttribute attribute) const;
  std::u16string ComputeAttributeUTF16(
      const ax::mojom::StringAttribute attribute) const;
  const std::vector<int32_t>& ComputeAttribute(
      const ax::mojom::IntListAttribute attribute) const;

  // Retrieves from the cache or computes the on-screen text that is found
  // inside the associated node and all its descendants, caches the result, and
  // returns a reference to the cached text.
  //
  // Takes into account any formatting changes, such as paragraph breaks, that
  // have been introduced by layout. For example, in the Web context,
  // "A<div>B</div>C" would produce "A\nB\nC". Note that since hidden elements
  // are not in the accessibility tree, they are not included in the result.
  const std::string& GetOrComputeTextContentWithParagraphBreaksUTF8() const;
  const std::u16string& GetOrComputeTextContentWithParagraphBreaksUTF16() const;

  // Retrieves from the cache or computes the on-screen text that is found
  // inside the associated node and all its descendants, caches the result, and
  // returns a reference to the cached text.
  //
  // Does not take into account any formatting changes, such as extra paragraph
  // breaks, that have been introduced by layout. For example, in the Web
  // context, "A<div>B</div>C" would produce "ABC".
  const std::string& GetOrComputeTextContentUTF8() const;
  const std::u16string& GetOrComputeTextContentUTF16() const;

  // Returns the length of the on-screen text that is found inside the
  // associated node and all its descendants. The text is either retrieved from
  // the cache, or computed and then cached.
  //
  // Does not take into account line breaks that have been introduced by layout.
  // For example, in the Web context, "A<div>B</div>C" would produce 3 and
  // not 5.
  int GetOrComputeTextContentLengthUTF8() const;
  int GetOrComputeTextContentLengthUTF16() const;

 private:
  // Computes and caches the `unignored_index_in_parent_`, `unignored_parent_`,
  // `unignored_child_count_` and `unignored_child_ids_` for the associated
  // node.
  void ComputeUnignoredValues(AXNodeID unignored_parent_id = kInvalidAXNodeID,
                              int starting_index_in_parent = 0) const;

  // Walks up the accessibility tree from the associated node until it finds the
  // lowest unignored ancestor.
  AXNode* SlowGetUnignoredParent() const;

  // Computes and caches (if not already in the cache) whether the associated
  // node is a descendant of a platform leaf. The set of platform leaves are the
  // lowest nodes that are exposed to the platform's assistive software.
  void ComputeIsDescendantOfPlatformLeaf() const;

  // Computes and caches (if not already in the cache) the character offsets
  // where each line in the associated node's on-screen text starts and ends.
  void ComputeLineOffsetsIfNeeded() const;

  // Computes and caches (if not already in the cache) the character offsets
  // where each sentence in the associated node's on-screen text starts and
  // ends.
  void ComputeSentenceOffsetsIfNeeded() const;

  // Computes and caches (if not already in the cache) the character offsets
  // where each word in the associated node's on-screen text starts and ends.
  // Note that the end offsets are always after the last character of each word,
  // so e.g. the end offset for the word "Hello" is 5, not 4.
  void ComputeWordOffsetsIfNeeded() const;

  // Computes the on-screen text that is found inside the associated node and
  // all its descendants.
  std::string ComputeTextContentUTF8() const;
  std::u16string ComputeTextContentUTF16() const;

  bool CanInferNameAttribute() const;

  // The node that is associated with this instance. Weak, owns us.
  const raw_ptr<const AXNode> owner_;

  mutable std::optional<int> unignored_index_in_parent_;
  mutable std::optional<AXNodeID> unignored_parent_id_;
  mutable std::optional<int> unignored_child_count_;
  mutable std::optional<std::vector<AXNodeID>> unignored_child_ids_;
  mutable std::optional<bool> is_descendant_of_leaf_;
  mutable std::optional<std::vector<int32_t>> line_starts_;
  mutable std::optional<std::vector<int32_t>> line_ends_;
  mutable std::optional<std::vector<int32_t>> sentence_starts_;
  mutable std::optional<std::vector<int32_t>> sentence_ends_;
  mutable std::optional<std::vector<int32_t>> word_starts_;
  mutable std::optional<std::vector<int32_t>> word_ends_;

  // There are two types of "text content". The first takes into
  // account any formatting changes, such as paragraph breaks, that have been
  // introduced by layout, whilst the other doesn't.
  //
  // Only one copy (either UTF8 or UTF16) should be cached as each platform
  // should only need one of the encodings. This applies to both text content as
  // well as text content with paragraph breaks.
  // TODO(kevers): Presently it is possible to get both cached since the bounds
  // calculations are done using UTF16 and text content can be extracted in
  // either format (platform specific). We should be able to remove the UTF16
  // extraction for bounds calculations now that the character bounds vector is
  // guaranteed to match the length of the text in UTF16. The CharacterWidths
  // method in AbstractInlineTextBox pads the vector in the event of the shaper
  // failing to return glyph metrics for all characters.
  mutable std::optional<std::string> text_content_with_paragraph_breaks_utf8_;
  mutable std::optional<std::u16string>
      text_content_with_paragraph_breaks_utf16_;
  mutable std::optional<std::string> text_content_utf8_;
  mutable std::optional<std::u16string> text_content_utf16_;
  // In rare cases, the length of the text content in UTF16 does not align with
  // the length of the character offsets array. Store the computed length to
  // avoid needing to cache the UTF16 representation of the text.
  // TODO(kevers): Remove once alignment is guaranteed.
  mutable std::optional<int32_t> utf16_length_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_COMPUTED_NODE_DATA_H_
