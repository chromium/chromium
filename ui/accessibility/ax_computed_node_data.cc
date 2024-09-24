// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_computed_node_data.h"

#include "base/check_op.h"
#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_manager_map.h"

namespace ui {

AXComputedNodeData::AXComputedNodeData(const AXNode& node) : owner_(&node) {}

AXComputedNodeData::~AXComputedNodeData() = default;

int AXComputedNodeData::GetOrComputeUnignoredIndexInParent() const {
  DCHECK(!owner_->IsIgnored())
      << "Ignored nodes cannot have an `unignored index in parent`.\n"
      << *owner_;
  if (unignored_index_in_parent_)
    return *unignored_index_in_parent_;

  if (const AXNode* unignored_parent = SlowGetUnignoredParent()) {
    DCHECK_NE(unignored_parent->id(), kInvalidAXNodeID)
        << "All nodes should have a valid ID.\n"
        << *owner_;
    unignored_parent->GetComputedNodeData().ComputeUnignoredValues();
  } else {
    // This should be the root node and, by convention, we assign it an
    // index-in-parent of 0.
    unignored_index_in_parent_ = 0;
    unignored_parent_id_ = kInvalidAXNodeID;
  }
  return *unignored_index_in_parent_;
}

AXNodeID AXComputedNodeData::GetOrComputeUnignoredParentID() const {
  if (unignored_parent_id_)
    return *unignored_parent_id_;

  if (const AXNode* unignored_parent = SlowGetUnignoredParent()) {
    DCHECK_NE(unignored_parent->id(), kInvalidAXNodeID)
        << "All nodes should have a valid ID.\n"
        << *owner_;
    unignored_parent_id_ = unignored_parent->id();
  } else {
    // This should be the root node and, by convention, we assign it an
    // index-in-parent of 0.
    DCHECK(!owner_->GetParent())
        << "If `unignored_parent` is nullptr, then this should be the "
           "rootnode, since in all trees the rootnode should be unignored.\n"
        << *owner_;
    unignored_index_in_parent_ = 0;
    unignored_parent_id_ = kInvalidAXNodeID;
  }
  return *unignored_parent_id_;
}

AXNode* AXComputedNodeData::GetOrComputeUnignoredParent() const {
  DCHECK(owner_->tree())
      << "All nodes should be owned by an accessibility tree.\n"
      << *owner_;
  return owner_->tree()->GetFromId(GetOrComputeUnignoredParentID());
}

int AXComputedNodeData::GetOrComputeUnignoredChildCount() const {
  DCHECK(!owner_->IsIgnored())
      << "Ignored nodes cannot have an `unignored child count`.\n"
      << *owner_;
  if (!unignored_child_count_)
    ComputeUnignoredValues();
  return *unignored_child_count_;
}

const std::vector<AXNodeID>& AXComputedNodeData::GetOrComputeUnignoredChildIDs()
    const {
  DCHECK(!owner_->IsIgnored())
      << "Ignored nodes cannot have `unignored child IDs`.\n"
      << *owner_;
  if (!unignored_child_ids_)
    ComputeUnignoredValues();
  return *unignored_child_ids_;
}

bool AXComputedNodeData::GetOrComputeIsDescendantOfPlatformLeaf() const {
  if (!is_descendant_of_leaf_)
    ComputeIsDescendantOfPlatformLeaf();
  return *is_descendant_of_leaf_;
}

const std::string& AXComputedNodeData::ComputeAttributeUTF8(
    const ax::mojom::StringAttribute attribute) const {
  DCHECK(owner_->CanComputeStringAttribute(attribute));
  switch (attribute) {
    case ax::mojom::StringAttribute::kValue:
      return GetOrComputeTextContentWithParagraphBreaksUTF8();

    case ax::mojom::StringAttribute::kName:
      return GetOrComputeTextContentUTF8();

    default:
      NOTREACHED_NORETURN();
  }
}

std::u16string AXComputedNodeData::ComputeAttributeUTF16(
    const ax::mojom::StringAttribute attribute) const {
  DCHECK(owner_->CanComputeStringAttribute(attribute));
  switch (attribute) {
    case ax::mojom::StringAttribute::kValue:
      return GetOrComputeTextContentWithParagraphBreaksUTF16();

    case ax::mojom::StringAttribute::kName:
      return GetOrComputeTextContentUTF16();

    default:
      NOTREACHED_NORETURN();
  }
}

const std::vector<int32_t>& AXComputedNodeData::ComputeAttribute(
    const ax::mojom::IntListAttribute attribute) const {
  DCHECK(owner_->CanComputeIntListAttribute(attribute));
  switch (attribute) {
    case ax::mojom::IntListAttribute::kLineStarts:
      ComputeLineOffsetsIfNeeded();
      return *line_starts_;

    case ax::mojom::IntListAttribute::kLineEnds:
      ComputeLineOffsetsIfNeeded();
      return *line_ends_;

    case ax::mojom::IntListAttribute::kSentenceStarts:
      ComputeSentenceOffsetsIfNeeded();
      return *sentence_starts_;

    case ax::mojom::IntListAttribute::kSentenceEnds:
      ComputeSentenceOffsetsIfNeeded();
      return *sentence_ends_;

    case ax::mojom::IntListAttribute::kWordStarts:
      ComputeWordOffsetsIfNeeded();
      return *word_starts_;

    case ax::mojom::IntListAttribute::kWordEnds:
      ComputeWordOffsetsIfNeeded();
      return *word_ends_;

    default:
      NOTREACHED_NORETURN();
  }
}

const std::string&
AXComputedNodeData::GetOrComputeTextContentWithParagraphBreaksUTF8() const {
  if (!text_content_with_paragraph_breaks_utf8_) {
    VLOG_IF(1, text_content_with_paragraph_breaks_utf16_)
        << "Only a single encoding of text content with paragraph breaks "
           "should be cached.";
    auto range =
        AXRange<AXPosition<AXNodePosition, AXNode>>::RangeOfContents(*owner_);
    text_content_with_paragraph_breaks_utf8_ = base::UTF16ToUTF8(
        range.GetText(AXTextConcatenationBehavior::kWithParagraphBreaks,
                      AXEmbeddedObjectBehavior::kSuppressCharacter));
  }
  return *text_content_with_paragraph_breaks_utf8_;
}

const std::u16string&
AXComputedNodeData::GetOrComputeTextContentWithParagraphBreaksUTF16() const {
  if (!text_content_with_paragraph_breaks_utf16_) {
    VLOG_IF(1, text_content_with_paragraph_breaks_utf8_)
        << "Only a single encoding of text content with paragraph breaks "
           "should be cached.";
    auto range =
        AXRange<AXPosition<AXNodePosition, AXNode>>::RangeOfContents(*owner_);
    text_content_with_paragraph_breaks_utf16_ =
        range.GetText(AXTextConcatenationBehavior::kWithParagraphBreaks,
                      AXEmbeddedObjectBehavior::kSuppressCharacter);
  }
  return *text_content_with_paragraph_breaks_utf16_;
}

const std::string& AXComputedNodeData::GetOrComputeTextContentUTF8() const {
  if (!text_content_utf8_) {
    VLOG_IF(1, text_content_utf16_)
        << "Only a single encoding of text content should be cached.";
    text_content_utf8_ = ComputeTextContentUTF8();
  }
  return *text_content_utf8_;
}

const std::u16string& AXComputedNodeData::GetOrComputeTextContentUTF16() const {
  if (!text_content_utf16_) {
    VLOG_IF(1, text_content_utf8_)
        << "Only a single encoding of text content should be cached.";
    text_content_utf16_ = ComputeTextContentUTF16();
  }
  return *text_content_utf16_;
}

int AXComputedNodeData::GetOrComputeTextContentLengthUTF8() const {
  return static_cast<int>(GetOrComputeTextContentUTF8().length());
}

int AXComputedNodeData::GetOrComputeTextContentLengthUTF16() const {
  if (utf16_length_) {
    return utf16_length_.value();
  }
  if (text_content_utf16_) {
    // Used the cached UTF16 representation if we have it already.
    utf16_length_ = text_content_utf16_->length();
  } else {
    // Do not cache the text since used just to extract the length.
    utf16_length_ = ComputeTextContentUTF16().length();
  }
  return utf16_length_.value();
}

bool AXComputedNodeData::CanInferNameAttribute() const {
  // The name may be suppressed when serializing an AXInlineTextBox if it
  // can be inferred from the parent.
  return ::features::IsAccessibilityPruneRedundantInlineTextEnabled() &&
         owner_->data().role == ax::mojom::Role::kInlineTextBox &&
         owner_->data().GetNameFrom() == ax::mojom::NameFrom::kContents &&
         owner_->GetParent()->data().GetNameFrom() ==
             ax::mojom::NameFrom::kContents &&
         owner_->GetParent()->data().HasStringAttribute(
             ax::mojom::StringAttribute::kName);
}

void AXComputedNodeData::ComputeUnignoredValues(
    AXNodeID unignored_parent_id,
    int starting_index_in_parent) const {
  DCHECK_GE(starting_index_in_parent, 0);
  // Reset any previously computed values.
  unignored_index_in_parent_ = std::nullopt;
  unignored_parent_id_ = std::nullopt;
  unignored_child_count_ = std::nullopt;
  unignored_child_ids_ = std::nullopt;

  AXNodeID unignored_parent_id_for_child = unignored_parent_id;
  if (!owner_->IsIgnored())
    unignored_parent_id_for_child = owner_->id();
  int unignored_child_count = 0;
  std::vector<AXNodeID> unignored_child_ids;
  for (auto iter = owner_->AllChildrenBegin(); iter != owner_->AllChildrenEnd();
       ++iter) {
    const AXComputedNodeData& computed_data = iter->GetComputedNodeData();
    int new_index_in_parent = starting_index_in_parent + unignored_child_count;

    if (iter->IsIgnored()) {
      // Skip the ignored node and recursively look at its children.
      computed_data.ComputeUnignoredValues(unignored_parent_id_for_child,
                                           new_index_in_parent);
      DCHECK(computed_data.unignored_child_count_);
      unignored_child_count += *computed_data.unignored_child_count_;
      DCHECK(computed_data.unignored_child_ids_);
      unignored_child_ids.insert(unignored_child_ids.end(),
                                 computed_data.unignored_child_ids_->begin(),
                                 computed_data.unignored_child_ids_->end());
    } else {
      // Setting `unignored_index_in_parent_` and `unignored_parent_id_` is the
      // responsibility of the parent node, since only the parent node can
      // calculate these values. This is in contrast to `unignored_child_count_`
      // and `unignored_child_ids_` that are only set if this method is called
      // on the node itself.
      computed_data.unignored_index_in_parent_ = new_index_in_parent;
      if (unignored_parent_id_for_child != kInvalidAXNodeID)
        computed_data.unignored_parent_id_ = unignored_parent_id_for_child;

      ++unignored_child_count;
      unignored_child_ids.push_back(iter->id());
    }
  }

  if (unignored_parent_id != kInvalidAXNodeID)
    unignored_parent_id_ = unignored_parent_id;
  // Ignored nodes store unignored child information in order to propagate it to
  // their parents, but do not expose it directly. The latter is guarded via a
  // DCHECK.
  unignored_child_count_ = unignored_child_count;
  unignored_child_ids_ = unignored_child_ids;
}

AXNode* AXComputedNodeData::SlowGetUnignoredParent() const {
  AXNode* unignored_parent = owner_->GetParent();
  while (unignored_parent && unignored_parent->IsIgnored())
    unignored_parent = unignored_parent->GetParent();
  return unignored_parent;
}

void AXComputedNodeData::ComputeIsDescendantOfPlatformLeaf() const {
  is_descendant_of_leaf_ = false;
  for (const AXNode* ancestor = GetOrComputeUnignoredParent(); ancestor;
       ancestor =
           ancestor->GetComputedNodeData().GetOrComputeUnignoredParent()) {
    if (ancestor->GetComputedNodeData().is_descendant_of_leaf_.value_or(
            false) ||
        ancestor->IsLeaf()) {
      is_descendant_of_leaf_ = true;
      return;
    }
  }
}

void AXComputedNodeData::ComputeLineOffsetsIfNeeded() const {
  DCHECK_EQ(line_starts_.has_value(), line_ends_.has_value());
  if (line_starts_) {
    DCHECK_EQ(line_starts_->size(), line_ends_->size());
    return;  // Already cached.
  }

  line_starts_ = std::vector<int32_t>();
  line_ends_ = std::vector<int32_t>();
  const std::u16string& text_content = GetOrComputeTextContentUTF16();
  if (text_content.empty())
    return;

  // TODO(nektar): Using the `base::i18n::BreakIterator` class is not enough. We
  // also need to pass information from Blink as to which inline text boxes
  // start a new line and deprecate next/previous_on_line.
  base::i18n::BreakIterator iter(text_content,
                                 base::i18n::BreakIterator::BREAK_NEWLINE);
  if (!iter.Init())
    return;

  while (iter.Advance()) {
    line_starts_->push_back(base::checked_cast<int32_t>(iter.prev()));
    line_ends_->push_back(base::checked_cast<int32_t>(iter.pos()));
  }
}

void AXComputedNodeData::ComputeSentenceOffsetsIfNeeded() const {
  DCHECK_EQ(sentence_starts_.has_value(), sentence_ends_.has_value());
  if (sentence_starts_) {
    DCHECK_EQ(sentence_starts_->size(), sentence_ends_->size());
    return;  // Already cached.
  }

  sentence_starts_ = std::vector<int32_t>();
  sentence_ends_ = std::vector<int32_t>();
  if (owner_->IsLineBreak()) {
    return;
  }

  const std::u16string& text_content = GetOrComputeTextContentUTF16();
  if (text_content.empty() ||
      base::ContainsOnlyChars(text_content, base::kWhitespaceUTF16)) {
    return;
  }

  // Unlike in ICU, a sentence boundary is not valid in Blink if it falls within
  // some whitespace that is used to separate sentences. We therefore need to
  // filter the boundaries returned by ICU and return a subset of them. For
  // example we should exclude a sentence boundary that is between two space
  // characters, "Hello. | there.".
  // TODO(nektar): The above is not accomplished simply by using the
  // `base::i18n::BreakIterator` class.
  base::i18n::BreakIterator iter(text_content,
                                 base::i18n::BreakIterator::BREAK_SENTENCE);
  if (!iter.Init())
    return;

  while (iter.Advance()) {
    sentence_starts_->push_back(base::checked_cast<int32_t>(iter.prev()));
    sentence_ends_->push_back(base::checked_cast<int32_t>(iter.pos()));
  }
}

void AXComputedNodeData::ComputeWordOffsetsIfNeeded() const {
  DCHECK_EQ(word_starts_.has_value(), word_ends_.has_value());
  if (word_starts_) {
    DCHECK_EQ(word_starts_->size(), word_ends_->size());
    return;  // Already cached.
  }

  word_starts_ = std::vector<int32_t>();
  word_ends_ = std::vector<int32_t>();
  const std::u16string& text_content = GetOrComputeTextContentUTF16();
  if (text_content.empty())
    return;

  // Unlike in ICU, a word boundary is valid in Blink only if it is before, or
  // immediately preceded by, an alphanumeric character, a series of punctuation
  // marks, an underscore or a line break. We therefore need to filter the
  // boundaries returned by ICU and return a subset of them. For example we
  // should exclude a word boundary that is between two space characters, "Hello
  // | there".
  // TODO(nektar): Fix the fact that the `base::i18n::BreakIterator` class does
  // not take into account underscores as word separators.
  base::i18n::BreakIterator iter(text_content,
                                 base::i18n::BreakIterator::BREAK_WORD);
  if (!iter.Init())
    return;

  while (iter.Advance()) {
    if (iter.IsWord()) {
      word_starts_->push_back(base::checked_cast<int>(iter.prev()));
      word_ends_->push_back(base::checked_cast<int>(iter.pos()));
    }
  }
}

std::string AXComputedNodeData::ComputeTextContentUTF8() const {
  // Name is omitted from an inline text if an only child since its value is
  // the same as the parent. We can differentiate this case from a specified
  // but empty name based on the name from attribute, which is kFromContent if
  // set and kNone if the text content is to be inferred from the parent.
  if (::features::IsAccessibilityPruneRedundantInlineTextEnabled()) {
    if (owner_->data().role == ax::mojom::Role::kInlineTextBox &&
        !owner_->data().HasStringAttribute(ax::mojom::StringAttribute::kName)) {
      return owner_->GetParent()->data().GetStringAttribute(
          ax::mojom::StringAttribute::kName);
    }
  }

  // If a text field has no descendants, then we compute its text content from
  // its value or its placeholder. Otherwise we prefer to look at its descendant
  // text nodes because Blink doesn't always add all trailing white space to the
  // value attribute.
  const bool is_atomic_text_field_without_descendants =
      (owner_->data().IsTextField() && !owner_->GetUnignoredChildCount());
  if (is_atomic_text_field_without_descendants) {
    std::string value =
        owner_->data().GetStringAttribute(ax::mojom::StringAttribute::kValue);
    // If the value is empty, then there might be some placeholder text in the
    // text field, or any other name that is derived from visible contents, even
    // if the text field has no children, so we treat this as any other leaf
    // node.
    if (!value.empty())
      return value;
  }

  // Ordinarily, atomic text fields are leaves, and for all leaves we directly
  // retrieve their text content using the information provided by the tree
  // source, such as Blink. However, for atomic text fields we need to exclude
  // them from the set of leaf nodes when they expose any descendants. This is
  // because we want to compute their text content from their descendant text
  // nodes as we don't always trust the "value" attribute provided by Blink.
  const bool is_atomic_text_field_with_descendants =
      (owner_->data().IsTextField() && owner_->GetUnignoredChildCount());
  if (owner_->IsLeaf() && !is_atomic_text_field_with_descendants) {
    switch (owner_->data().GetNameFrom()) {
      case ax::mojom::NameFrom::kNone:
      // The accessible name is not displayed on screen, e.g. aria-label, or is
      // not displayed directly inside the node, e.g. an associated label
      // element.
      case ax::mojom::NameFrom::kAttribute:
      // The node's accessible name is explicitly empty.
      case ax::mojom::NameFrom::kAttributeExplicitlyEmpty:
      // The accessible name does not represent the entirety of the node's text
      // content, e.g. a table's caption or a figure's figcaption.
      case ax::mojom::NameFrom::kCaption:
      // The name comes from CSS alt text.
      case ax::mojom::NameFrom::kCssAltText:
      // The object should not have an accessible name according to ARIA 1.2.
      // If kProhibited is set, that means we calculated a name in Blink and
      // are deliberately not exposing it.
      case ax::mojom::NameFrom::kProhibited:
      case ax::mojom::NameFrom::kProhibitedAndRedundant:
      case ax::mojom::NameFrom::kRelatedElement:
      // The accessible name is not displayed directly inside the node but is
      // visible via e.g. a tooltip.
      case ax::mojom::NameFrom::kTitle:
      case ax::mojom::NameFrom::kPopoverAttribute:
        return std::string();

      case ax::mojom::NameFrom::kContents:
      // The placeholder text is initially displayed inside the text field and
      // takes the place of its value.
      case ax::mojom::NameFrom::kPlaceholder:
      // The value attribute takes the place of the node's text content, e.g.
      // the value of a submit button is displayed inside the button itself.
      case ax::mojom::NameFrom::kValue:
        return owner_->data().GetStringAttribute(
            ax::mojom::StringAttribute::kName);
    }
  }

  std::string text_content;
  for (auto it = owner_->UnignoredChildrenCrossingTreeBoundaryBegin();
       it != owner_->UnignoredChildrenCrossingTreeBoundaryEnd(); ++it) {
    text_content += it->GetTextContentUTF8();
  }
  return text_content;
}

std::u16string AXComputedNodeData::ComputeTextContentUTF16() const {
  return base::UTF8ToUTF16(ComputeTextContentUTF8());
}

}  // namespace ui
