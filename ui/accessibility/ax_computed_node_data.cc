// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_computed_node_data.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_manager_map.h"

namespace ui {

AXComputedNodeData::AXComputedNodeData(const AXNode& node) : owner_(&node) {}

AXComputedNodeData::~AXComputedNodeData() = default;

bool AXComputedNodeData::HasOrCanComputeAttribute(
    const ax::mojom::StringAttribute attribute) const {
  DCHECK(owner_);
  if (owner_->data().HasStringAttribute(attribute))
    return true;

  switch (attribute) {
    case ax::mojom::StringAttribute::kValue:
      // The value attribute could be computed on the browser for content
      // editables and ARIA text/search boxes.
      return owner_->data().IsNonAtomicTextField();
    default:
      return false;
  }
}

const std::string& AXComputedNodeData::GetOrComputeAttributeUTF8(
    const ax::mojom::StringAttribute attribute) const {
  DCHECK(owner_);
  if (owner_->data().HasStringAttribute(attribute))
    return owner_->data().GetStringAttribute(attribute);

  switch (attribute) {
    case ax::mojom::StringAttribute::kValue:
      if (owner_->data().IsNonAtomicTextField()) {
        DCHECK(HasOrCanComputeAttribute(attribute))
            << "Code in `HasOrCanComputeAttribute` should be in sync with "
               "'GetOrComputeAttributeUTF8`";
        return GetOrComputeInnerTextUTF8();
      }
      return base::EmptyString();
    default:
      // This is a special case: for performance reasons do not use
      // `base::EmptyString()` in other places throughout the codebase.
      return base::EmptyString();
  }
}

std::u16string AXComputedNodeData::GetOrComputeAttributeUTF16(
    const ax::mojom::StringAttribute attribute) const {
  return base::UTF8ToUTF16(GetOrComputeAttributeUTF8(attribute));
}

const std::string& AXComputedNodeData::GetOrComputeInnerTextUTF8() const {
  if (!inner_text_utf8_) {
    VLOG_IF(1, inner_text_utf16_)
        << "Only a single encoding of inner text should be cached.";
    inner_text_utf8_ = ComputeInnerTextUTF8();
  }
  return *inner_text_utf8_;
}

const std::u16string& AXComputedNodeData::GetOrComputeInnerTextUTF16() const {
  if (!inner_text_utf16_) {
    VLOG_IF(1, inner_text_utf8_)
        << "Only a single encoding of inner text should be cached.";
    inner_text_utf16_ = ComputeInnerTextUTF16();
  }
  return *inner_text_utf16_;
}

int AXComputedNodeData::GetOrComputeInnerTextLengthUTF8() const {
  return static_cast<int>(GetOrComputeInnerTextUTF8().length());
}

int AXComputedNodeData::GetOrComputeInnerTextLengthUTF16() const {
  return static_cast<int>(GetOrComputeInnerTextUTF16().length());
}

std::string AXComputedNodeData::ComputeInnerTextUTF8() const {
  DCHECK(owner_);

  // If a text field has no descendants, then we compute its inner text from its
  // value or its placeholder. Otherwise we prefer to look at its descendant
  // text nodes because Blink doesn't always add all trailing white space to the
  // value attribute.
  const bool is_plain_text_field_without_descendants =
      (owner_->data().IsTextField() &&
       !owner_->GetUnignoredChildCountCrossingTreeBoundary());
  if (is_plain_text_field_without_descendants) {
    std::string value =
        owner_->data().GetStringAttribute(ax::mojom::StringAttribute::kValue);
    // If the value is empty, then there might be some placeholder text in the
    // text field, or any other name that is derived from visible contents, even
    // if the text field has no children.
    if (!value.empty())
      return value;
  }

  // Ordinarily, plain text fields are leaves. We need to exclude them from the
  // set of leaf nodes when they expose any descendants. This is because we want
  // to compute their inner text from their descendant text nodes as we don't
  // always trust the "value" attribute provided by Blink.
  const bool is_plain_text_field_with_descendants =
      (owner_->data().IsTextField() &&
       owner_->GetUnignoredChildCountCrossingTreeBoundary());
  if (owner_->IsLeaf() && !is_plain_text_field_with_descendants) {
    switch (owner_->data().GetNameFrom()) {
      case ax::mojom::NameFrom::kNone:
      case ax::mojom::NameFrom::kUninitialized:
      // The accessible name is not displayed on screen, e.g. aria-label, or is
      // not displayed directly inside the node, e.g. an associated label
      // element.
      case ax::mojom::NameFrom::kAttribute:
      // The node's accessible name is explicitly empty.
      case ax::mojom::NameFrom::kAttributeExplicitlyEmpty:
      // The accessible name does not represent the entirety of the node's inner
      // text, e.g. a table's caption or a figure's figcaption.
      case ax::mojom::NameFrom::kCaption:
      case ax::mojom::NameFrom::kRelatedElement:
      // The accessible name is not displayed directly inside the node but is
      // visible via e.g. a tooltip.
      case ax::mojom::NameFrom::kTitle:
        return std::string();

      case ax::mojom::NameFrom::kContents:
      // The placeholder text is initially displayed inside the text field and
      // takes the place of its value.
      case ax::mojom::NameFrom::kPlaceholder:
      // The value attribute takes the place of the node's inner text, e.g. the
      // value of a submit button is displayed inside the button itself.
      case ax::mojom::NameFrom::kValue:
        return owner_->data().GetStringAttribute(
            ax::mojom::StringAttribute::kName);
    }
  }

  std::string inner_text;
  for (auto it = owner_->UnignoredChildrenCrossingTreeBoundaryBegin();
       it != owner_->UnignoredChildrenCrossingTreeBoundaryEnd(); ++it) {
    inner_text += it->GetInnerText();
  }
  return inner_text;
}

std::u16string AXComputedNodeData::ComputeInnerTextUTF16() const {
  return base::UTF8ToUTF16(ComputeInnerTextUTF8());
}

}  // namespace ui
