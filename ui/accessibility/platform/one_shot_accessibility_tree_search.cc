// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/one_shot_accessibility_tree_search.h"

#include <stdint.h>

#include <string>

#include "base/containers/contains.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"

namespace ui {

// Given a node, populate a vector with all of the strings from that node's
// attributes that might be relevant for a text search.
void GetNodeStrings(BrowserAccessibility* node,
                    std::vector<std::u16string>* strings) {
  std::u16string value;
  if (node->GetString16Attribute(ax::mojom::StringAttribute::kName, &value))
    strings->push_back(value);
  if (node->GetString16Attribute(ax::mojom::StringAttribute::kDescription,
                                 &value)) {
    strings->push_back(value);
  }
  value = node->GetValueForControl();
  if (!value.empty())
    strings->push_back(value);
}

OneShotAccessibilityTreeSearch::OneShotAccessibilityTreeSearch(
    BrowserAccessibility* scope)
    : tree_(scope->manager()),
      scope_node_(scope),
      start_node_(scope),
      direction_(OneShotAccessibilityTreeSearch::FORWARDS),
      result_limit_(UNLIMITED_RESULTS),
      immediate_descendants_only_(false),
      can_wrap_to_last_element_(false),
      onscreen_only_(false),
      did_search_(false) {}

OneShotAccessibilityTreeSearch::~OneShotAccessibilityTreeSearch() = default;

void OneShotAccessibilityTreeSearch::SetStartNode(
    BrowserAccessibility* start_node) {
  DCHECK(!did_search_);
  CHECK(start_node);

  if (!scope_node_->PlatformGetParent() ||
      start_node->IsDescendantOf(scope_node_->PlatformGetParent())) {
    start_node_ = start_node;
  }
}

void OneShotAccessibilityTreeSearch::SetDirection(Direction direction) {
  DCHECK(!did_search_);
  direction_ = direction;
}

void OneShotAccessibilityTreeSearch::SetResultLimit(int result_limit) {
  DCHECK(!did_search_);
  result_limit_ = result_limit;
}

void OneShotAccessibilityTreeSearch::SetImmediateDescendantsOnly(
    bool immediate_descendants_only) {
  DCHECK(!did_search_);
  immediate_descendants_only_ = immediate_descendants_only;
}

void OneShotAccessibilityTreeSearch::SetCanWrapToLastElement(
    bool can_wrap_to_last_element) {
  DCHECK(!did_search_);
  can_wrap_to_last_element_ = can_wrap_to_last_element;
}

void OneShotAccessibilityTreeSearch::SetOnscreenOnly(bool onscreen_only) {
  DCHECK(!did_search_);
  onscreen_only_ = onscreen_only;
}

void OneShotAccessibilityTreeSearch::SetSearchText(const std::string& text) {
  DCHECK(!did_search_);
  search_text_ = text;
}

void OneShotAccessibilityTreeSearch::AddPredicate(
    AccessibilityMatchPredicate predicate) {
  DCHECK(!did_search_);
  predicates_.push_back(predicate);
}

size_t OneShotAccessibilityTreeSearch::CountMatches() {
  if (!did_search_)
    Search();

  return matches_.size();
}

BrowserAccessibility* OneShotAccessibilityTreeSearch::GetMatchAtIndex(
    size_t index) {
  if (!did_search_)
    Search();

  CHECK(index < matches_.size());
  return matches_[index];
}

void OneShotAccessibilityTreeSearch::Search() {
  if (immediate_descendants_only_) {
    SearchByIteratingOverChildren();
  } else {
    SearchByWalkingTree();
  }
  did_search_ = true;
}

void OneShotAccessibilityTreeSearch::SearchByIteratingOverChildren() {
  // Iterate over the children of scope_node_.
  // If start_node_ is specified, iterate over the first child past that
  // node.

  size_t count = scope_node_->PlatformChildCount();
  if (count == 0)
    return;

  // We only care about immediate children of scope_node_, so walk up
  // start_node_ until we get to an immediate child. If it isn't a child,
  // we ignore start_node_.
  while (start_node_ && start_node_->PlatformGetParent() != scope_node_)
    start_node_ = start_node_->PlatformGetParent();

  size_t index = (direction_ == FORWARDS ? 0 : count - 1);
  if (start_node_) {
    index = start_node_->GetIndexInParent().value();
    if (direction_ == FORWARDS)
      index++;
    else
      index--;
  }

  while (index < count && (result_limit_ == UNLIMITED_RESULTS ||
                           static_cast<int>(matches_.size()) < result_limit_)) {
    BrowserAccessibility* node = scope_node_->PlatformGetChild(index);
    if (Matches(node))
      matches_.push_back(node);

    if (direction_ == FORWARDS)
      index++;
    else
      index--;
  }
}

void OneShotAccessibilityTreeSearch::SearchByWalkingTree() {
  BrowserAccessibility* node = nullptr;
  node = start_node_;
  if (node != scope_node_ || result_limit_ == 1) {
    if (direction_ == FORWARDS)
      node = tree_->NextInTreeOrder(start_node_);
    else
      node = tree_->PreviousInTreeOrder(start_node_, can_wrap_to_last_element_);
  }

  BrowserAccessibility* stop_node = scope_node_->PlatformGetParent();
  while (node && node != stop_node &&
         (result_limit_ == UNLIMITED_RESULTS ||
          static_cast<int>(matches_.size()) < result_limit_)) {
    if (Matches(node))
      matches_.push_back(node);

    if (direction_ == FORWARDS) {
      node = tree_->NextInTreeOrder(node);
    } else {
      // This needs to be handled carefully. If not, there is a chance of
      // getting into infinite loop.
      if (can_wrap_to_last_element_ && !stop_node &&
          node->manager()->GetBrowserAccessibilityRoot() == node) {
        stop_node = node;
      }
      node = tree_->PreviousInTreeOrder(node, can_wrap_to_last_element_);
    }
  }
}

bool OneShotAccessibilityTreeSearch::Matches(BrowserAccessibility* node) {
  if (!predicates_.empty()) {
    bool found_predicate_match = false;
    for (auto predicate : predicates_) {
      if (predicate(start_node_, node)) {
        found_predicate_match = true;
        break;
      }
    }
    if (!found_predicate_match)
      return false;
  }

  if (node->IsInvisibleOrIgnored())
    return false;  // Programmatically hidden, e.g. aria-hidden or via CSS.

  if (onscreen_only_ && node->IsOffscreen())
    return false;  // Partly scrolled offscreen.

  if (!search_text_.empty()) {
    std::u16string search_text_lower =
        base::i18n::ToLower(base::UTF8ToUTF16(search_text_));
    std::vector<std::u16string> node_strings;
    GetNodeStrings(node, &node_strings);
    bool found_text_match = false;
    for (auto node_string : node_strings) {
      std::u16string node_string_lower = base::i18n::ToLower(node_string);
      if (base::Contains(node_string_lower, search_text_lower)) {
        found_text_match = true;
        break;
      }
    }
    if (!found_text_match)
      return false;
  }

  return true;
}

//
// Predicates
//

bool AccessibilityArticlePredicate(BrowserAccessibility* start,
                                   BrowserAccessibility* node) {
  return node->GetRole() == ax::mojom::Role::kArticle;
}

bool AccessibilityButtonPredicate(BrowserAccessibility* start,
                                  BrowserAccessibility* node) {
  switch (node->GetRole()) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kToggleButton:
      return true;
    default:
      return false;
  }
}

bool AccessibilityBlockquotePredicate(BrowserAccessibility* start,
                                      BrowserAccessibility* node) {
  return node->GetRole() == ax::mojom::Role::kBlockquote;
}

bool AccessibilityCheckboxPredicate(BrowserAccessibility* start,
                                    BrowserAccessibility* node) {
  return (node->GetRole() == ax::mojom::Role::kCheckBox ||
          node->GetRole() == ax::mojom::Role::kMenuItemCheckBox);
}

bool AccessibilityComboboxPredicate(BrowserAccessibility* start,
                                    BrowserAccessibility* node) {
  return (node->GetRole() == ax::mojom::Role::kComboBoxGrouping ||
          node->GetRole() == ax::mojom::Role::kComboBoxMenuButton ||
          node->GetRole() == ax::mojom::Role::kTextFieldWithComboBox ||
          node->GetRole() == ax::mojom::Role::kComboBoxSelect);
}

bool AccessibilityControlPredicate(BrowserAccessibility* start,
                                   BrowserAccessibility* node) {
  if (IsControl(node->GetRole())) {
    return true;
  }
  if (node->HasState(ax::mojom::State::kFocusable) &&
      node->GetRole() != ax::mojom::Role::kIframe &&
      node->GetRole() != ax::mojom::Role::kIframePresentational &&
      !IsLink(node->GetRole()) && !IsPlatformDocument(node->GetRole())) {
    return true;
  }
  return false;
}

bool AccessibilityFocusablePredicate(BrowserAccessibility* start,
                                     BrowserAccessibility* node) {
  bool focusable = node->HasState(ax::mojom::State::kFocusable);
  if (IsIframe(node->GetRole()) || IsPlatformDocument(node->GetRole())) {
    focusable = false;
  }
  return focusable;
}

bool AccessibilityGraphicPredicate(BrowserAccessibility* start,
                                   BrowserAccessibility* node) {
  return IsImageOrVideo(node->GetRole());
}

bool AccessibilityHeadingPredicate(BrowserAccessibility* start,
                                   BrowserAccessibility* node) {
  return IsHeading(node->GetRole());
}

bool AccessibilityH1Predicate(BrowserAccessibility* start,
                              BrowserAccessibility* node) {
  return (IsHeading(node->GetRole()) &&
          node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
              1);
}

bool AccessibilityH2Predicate(BrowserAccessibility* start,
                              BrowserAccessibility* node) {
  return (IsHeading(node->GetRole()) &&
          node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
              2);
}

bool AccessibilityH3Predicate(BrowserAccessibility* start,
                              BrowserAccessibility* node) {
  return (IsHeading(node->GetRole()) &&
          node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
              3);
}

bool AccessibilityH4Predicate(BrowserAccessibility* start,
                              BrowserAccessibility* node) {
  return (IsHeading(node->GetRole()) &&
          node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
              4);
}

bool AccessibilityH5Predicate(BrowserAccessibility* start,
                              BrowserAccessibility* node) {
  return (IsHeading(node->GetRole()) &&
          node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
              5);
}

bool AccessibilityH6Predicate(BrowserAccessibility* start,
                              BrowserAccessibility* node) {
  return (IsHeading(node->GetRole()) &&
          node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
              6);
}

bool AccessibilityHeadingSameLevelPredicate(BrowserAccessibility* start,
                                            BrowserAccessibility* node) {
  return (
      node->GetRole() == ax::mojom::Role::kHeading &&
      start->GetRole() == ax::mojom::Role::kHeading &&
      (node->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel) ==
       start->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel)));
}

bool AccessibilityFramePredicate(BrowserAccessibility* start,
                                 BrowserAccessibility* node) {
  if (node->IsRootWebAreaForPresentationalIframe())
    return false;
  if (!node->PlatformGetParent())
    return false;
  return IsPlatformDocument(node->GetRole());
}

bool AccessibilityLandmarkPredicate(BrowserAccessibility* start,
                                    BrowserAccessibility* node) {
  switch (node->GetRole()) {
    case ax::mojom::Role::kApplication:
    case ax::mojom::Role::kArticle:
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kComplementary:
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kFooter:
    case ax::mojom::Role::kForm:
    case ax::mojom::Role::kHeader:
    case ax::mojom::Role::kMain:
    case ax::mojom::Role::kNavigation:
    case ax::mojom::Role::kRegion:
    case ax::mojom::Role::kSearch:
    case ax::mojom::Role::kSection:
      return true;
    default:
      return false;
  }
}

bool AccessibilityLinkPredicate(BrowserAccessibility* start,
                                BrowserAccessibility* node) {
  return IsLink(node->GetRole());
}

bool AccessibilityListPredicate(BrowserAccessibility* start,
                                BrowserAccessibility* node) {
  return IsList(node->GetRole());
}

bool AccessibilityListItemPredicate(BrowserAccessibility* start,
                                    BrowserAccessibility* node) {
  return IsListItem(node->GetRole());
}

bool AccessibilityLiveRegionPredicate(BrowserAccessibility* start,
                                      BrowserAccessibility* node) {
  return node->HasStringAttribute(ax::mojom::StringAttribute::kLiveStatus);
}

bool AccessibilityMainPredicate(BrowserAccessibility* start,
                                BrowserAccessibility* node) {
  return (node->GetRole() == ax::mojom::Role::kMain);
}

bool AccessibilityMediaPredicate(BrowserAccessibility* start,
                                 BrowserAccessibility* node) {
  const std::string& tag =
      node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
  return tag == "audio" || tag == "video";
}

bool AccessibilityParagraphPredicate(BrowserAccessibility* start,
                                     BrowserAccessibility* node) {
  // Since paragraphs can contain other nodes, we exclude ancestors of the start
  // node from the search
  return node->GetRole() == ax::mojom::Role::kParagraph &&
         !start->IsDescendantOf(node);
}

bool AccessibilityRadioButtonPredicate(BrowserAccessibility* start,
                                       BrowserAccessibility* node) {
  return (node->GetRole() == ax::mojom::Role::kRadioButton ||
          node->GetRole() == ax::mojom::Role::kMenuItemRadio);
}

bool AccessibilityRadioGroupPredicate(BrowserAccessibility* start,
                                      BrowserAccessibility* node) {
  return node->GetRole() == ax::mojom::Role::kRadioGroup;
}

bool AccessibilitySectionPredicate(BrowserAccessibility* start,
                                   BrowserAccessibility* node) {
  return AccessibilityLandmarkPredicate(start, node) ||
         node->GetRole() == ax::mojom::Role::kHeading;
}

bool AccessibilityTablePredicate(BrowserAccessibility* start,
                                 BrowserAccessibility* node) {
  return IsTableLike(node->GetRole());
}

bool AccessibilityTextfieldPredicate(BrowserAccessibility* start,
                                     BrowserAccessibility* node) {
  return node->IsTextField();
}

bool AccessibilityTextStyleBoldPredicate(BrowserAccessibility* start,
                                         BrowserAccessibility* node) {
  return node->HasTextStyle(ax::mojom::TextStyle::kBold);
}

bool AccessibilityTextStyleItalicPredicate(BrowserAccessibility* start,
                                           BrowserAccessibility* node) {
  return node->HasTextStyle(ax::mojom::TextStyle::kItalic);
}

bool AccessibilityTextStyleUnderlinePredicate(BrowserAccessibility* start,
                                              BrowserAccessibility* node) {
  return node->HasTextStyle(ax::mojom::TextStyle::kUnderline);
}

bool AccessibilityTreePredicate(BrowserAccessibility* start,
                                BrowserAccessibility* node) {
  return node->IsTextField();
}

bool AccessibilityUnvisitedLinkPredicate(BrowserAccessibility* start,
                                         BrowserAccessibility* node) {
  return node->GetRole() == ax::mojom::Role::kLink &&
         !node->HasState(ax::mojom::State::kVisited);
}

bool AccessibilityVisitedLinkPredicate(BrowserAccessibility* start,
                                       BrowserAccessibility* node) {
  return node->GetRole() == ax::mojom::Role::kLink &&
         node->HasState(ax::mojom::State::kVisited);
}

}  // namespace ui
