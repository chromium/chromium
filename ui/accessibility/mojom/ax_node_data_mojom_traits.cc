// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/mojom/ax_node_data_mojom_traits.h"

#include "base/containers/flat_map.h"
#include "ui/accessibility/mojom/ax_relative_bounds.mojom-shared.h"
#include "ui/accessibility/mojom/ax_relative_bounds_mojom_traits.h"

namespace mojo {
namespace {
bool HasAnyHighlightEntries(std::vector<int32_t>& marker_types) {
  for (auto marker : marker_types) {
    if (marker & static_cast<int32_t>(ax::mojom::MarkerType::kHighlight)) {
      // Can stop looking once we know there is one highlight.
      return true;
    }
  }
  return false;
}

bool IsValidEnumIntAttribute(ax::mojom::IntAttribute attribute, int32_t value) {
  switch (attribute) {
    case ax::mojom::IntAttribute::kDefaultActionVerb:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::DefaultActionVerb>(value));
    case ax::mojom::IntAttribute::kSortDirection:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::SortDirection>(value));
    case ax::mojom::IntAttribute::kNameFrom:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::NameFrom>(value));
    case ax::mojom::IntAttribute::kDescriptionFrom:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::DescriptionFrom>(value));
    case ax::mojom::IntAttribute::kRestriction:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::Restriction>(value));
    case ax::mojom::IntAttribute::kAriaCurrentState:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::AriaCurrentState>(value));
    case ax::mojom::IntAttribute::kHasPopup:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::HasPopup>(value));
    case ax::mojom::IntAttribute::kImageAnnotationStatus:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::ImageAnnotationStatus>(value));
    case ax::mojom::IntAttribute::kInvalidState:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::InvalidState>(value));
    case ax::mojom::IntAttribute::kCheckedState:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::CheckedState>(value));
    case ax::mojom::IntAttribute::kListStyle:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::ListStyle>(value));
    case ax::mojom::IntAttribute::kTextAlign:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::TextAlign>(value));
    case ax::mojom::IntAttribute::kTextDirection:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::WritingDirection>(value));
    case ax::mojom::IntAttribute::kTextPosition:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::TextPosition>(value));
    case ax::mojom::IntAttribute::kTextStyle: {
      constexpr uint32_t kMaxValidBits =
          (1U << (static_cast<uint32_t>(ax::mojom::TextStyle::kMaxValue) + 1)) -
          1;
      return (static_cast<uint32_t>(value) & ~kMaxValidBits) == 0;
    }
    case ax::mojom::IntAttribute::kTextOverlineStyle:
    case ax::mojom::IntAttribute::kTextStrikethroughStyle:
    case ax::mojom::IntAttribute::kTextUnderlineStyle:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::TextDecorationStyle>(value));
    case ax::mojom::IntAttribute::kIsPopup:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::IsPopup>(value));
    case ax::mojom::IntAttribute::kAriaNotificationInterruptDeprecated:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::AriaNotificationInterrupt>(value));
    case ax::mojom::IntAttribute::kAriaNotificationPriorityDeprecated:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::AriaNotificationPriority>(value));
    case ax::mojom::IntAttribute::kDetailsFrom:
      return ax::mojom::IsKnownEnumValue(
          static_cast<ax::mojom::DetailsFrom>(value));

    case ax::mojom::IntAttribute::kNone:
    case ax::mojom::IntAttribute::kScrollX:
    case ax::mojom::IntAttribute::kScrollXMin:
    case ax::mojom::IntAttribute::kScrollXMax:
    case ax::mojom::IntAttribute::kScrollY:
    case ax::mojom::IntAttribute::kScrollYMin:
    case ax::mojom::IntAttribute::kScrollYMax:
    case ax::mojom::IntAttribute::kTextSelStart:
    case ax::mojom::IntAttribute::kTextSelEnd:
    case ax::mojom::IntAttribute::kAriaColumnCount:
    case ax::mojom::IntAttribute::kAriaCellColumnIndex:
    case ax::mojom::IntAttribute::kAriaCellColumnSpan:
    case ax::mojom::IntAttribute::kAriaRowCount:
    case ax::mojom::IntAttribute::kAriaCellRowIndex:
    case ax::mojom::IntAttribute::kAriaCellRowSpan:
    case ax::mojom::IntAttribute::kTableRowCount:
    case ax::mojom::IntAttribute::kTableColumnCount:
    case ax::mojom::IntAttribute::kTableHeaderId:
    case ax::mojom::IntAttribute::kTableRowIndex:
    case ax::mojom::IntAttribute::kTableRowHeaderId:
    case ax::mojom::IntAttribute::kTableColumnIndex:
    case ax::mojom::IntAttribute::kTableColumnHeaderId:
    case ax::mojom::IntAttribute::kTableCellColumnIndex:
    case ax::mojom::IntAttribute::kTableCellColumnSpan:
    case ax::mojom::IntAttribute::kTableCellRowIndex:
    case ax::mojom::IntAttribute::kTableCellRowSpan:
    case ax::mojom::IntAttribute::kHierarchicalLevel:
    case ax::mojom::IntAttribute::kActivedescendantId:
    case ax::mojom::IntAttribute::kErrormessageIdDeprecated:
    case ax::mojom::IntAttribute::kInPageLinkTargetId:
    case ax::mojom::IntAttribute::kMemberOfId:
    case ax::mojom::IntAttribute::kNextOnLineId:
    case ax::mojom::IntAttribute::kPopupForId:
    case ax::mojom::IntAttribute::kPreviousOnLineId:
    case ax::mojom::IntAttribute::kSetSize:
    case ax::mojom::IntAttribute::kPosInSet:
    case ax::mojom::IntAttribute::kColorValue:
    case ax::mojom::IntAttribute::kBackgroundColor:
    case ax::mojom::IntAttribute::kColor:
    case ax::mojom::IntAttribute::kPreviousFocusId:
    case ax::mojom::IntAttribute::kNextFocusId:
    case ax::mojom::IntAttribute::kDropeffectDeprecated:
    case ax::mojom::IntAttribute::kDOMNodeIdDeprecated:
    case ax::mojom::IntAttribute::kNextWindowFocusId:
    case ax::mojom::IntAttribute::kPreviousWindowFocusId:
    case ax::mojom::IntAttribute::kMaxLength:
    case ax::mojom::IntAttribute::kPaintOrder:
    case ax::mojom::IntAttribute::kCommittedTextLength:
      return true;
  }
  return true;
}
}  // namespace

// static
bool StructTraits<ax::mojom::AXBitsetDataDataView,
                  ui::AXBitset<ax::mojom::BoolAttribute>>::
    Read(ax::mojom::AXBitsetDataDataView data,
         ui::AXBitset<ax::mojom::BoolAttribute>* out) {
  *out = ui::AXBitset<ax::mojom::BoolAttribute>(data.set_bits(), data.values());
  return true;
}

// static
bool StructTraits<ax::mojom::AXNodeDataDataView, ui::AXNodeData>::Read(
    ax::mojom::AXNodeDataDataView data,
    ui::AXNodeData* out) {
  out->id = data.id();
  out->role = data.role();
  out->state = ui::AXStates(data.state());
  out->actions = data.actions();

  if (!data.ReadStringAttributes(&out->string_attributes.container())) {
    return false;
  }
  if (!data.ReadIntAttributes(&out->int_attributes.container())) {
    return false;
  }
  for (const auto& [attr, value] : out->int_attributes.container()) {
    if (!IsValidEnumIntAttribute(attr, value)) {
      return false;
    }
  }
  if (!data.ReadFloatAttributes(&out->float_attributes.container())) {
    return false;
  }

  std::optional<ui::AXBitset<ax::mojom::BoolAttribute>> bitset_from_mojo;
  if (!data.ReadBoolAttributesData(&bitset_from_mojo)) {
    return false;
  }

  if (bitset_from_mojo.has_value()) {
    out->bool_attributes = bitset_from_mojo.value();
  }

  auto& intlist_attributes = out->intlist_attributes.container();
  if (!data.ReadIntlistAttributes(&intlist_attributes)) {
    return false;
  }

  // Enforce some invariants:
  //  If marker types are present, marker starts and ends must be present.
  //  If any marker type is a highlight, highlights must be present.
  if (auto types_it =
          intlist_attributes.find(ax::mojom::IntListAttribute::kMarkerTypes);
      types_it != intlist_attributes.end()) {
    auto starts_it =
        intlist_attributes.find(ax::mojom::IntListAttribute::kMarkerStarts);
    if (starts_it == intlist_attributes.end()) {
      return false;
    }
    auto ends_it =
        intlist_attributes.find(ax::mojom::IntListAttribute::kMarkerEnds);
    if (ends_it == intlist_attributes.end()) {
      return false;
    }
    auto& marker_types = types_it->second;
    auto& marker_starts = starts_it->second;
    auto& marker_ends = ends_it->second;
    if (marker_types.size() != marker_starts.size() ||
        marker_types.size() != marker_ends.size()) {
      return false;
    }
    if (HasAnyHighlightEntries(marker_types)) {
      auto highlight_types_it =
          intlist_attributes.find(ax::mojom::IntListAttribute::kHighlightTypes);
      if (highlight_types_it == intlist_attributes.end()) {
        return false;
      }
      auto& highlight_types = highlight_types_it->second;
      if (marker_types.size() != highlight_types.size()) {
        return false;
      }
    }
  }

  if (!data.ReadStringlistAttributes(&out->stringlist_attributes.container())) {
    return false;
  }

  base::flat_map<std::string, std::string> html_attributes;
  if (!data.ReadHtmlAttributes(&html_attributes))
    return false;
  out->html_attributes = std::move(html_attributes).extract();

  if (!data.ReadChildIds(&out->child_ids)) {
    return false;
  }

  if (!data.ReadRelativeBounds(&out->relative_bounds))
    return false;

  return true;
}

}  // namespace mojo
