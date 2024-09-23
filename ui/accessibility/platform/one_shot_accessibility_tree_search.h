// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_ONE_SHOT_ACCESSIBILITY_TREE_SEARCH_H_
#define UI_ACCESSIBILITY_PLATFORM_ONE_SHOT_ACCESSIBILITY_TREE_SEARCH_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

namespace ui {
class BrowserAccessibilityManager;
class BrowserAccessibility;


// A function that returns whether or not a given node matches, given the
// start element of the search as an optional comparator.
typedef bool (*AccessibilityMatchPredicate)(
    BrowserAccessibility* start_element,
    BrowserAccessibility* this_element);

#define DECLARE_ACCESSIBILITY_PREDICATE(PredicateName)                   \
  COMPONENT_EXPORT(AX_PLATFORM) bool PredicateName(                      \
                                    BrowserAccessibility* start_element, \
                                    BrowserAccessibility* this_element)

DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityArticlePredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityBlockquotePredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityButtonPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityCheckboxPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityComboboxPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityControlPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityFocusablePredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityFramePredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityGraphicPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityH1Predicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityH2Predicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityH3Predicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityH4Predicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityH5Predicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityH6Predicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityHeadingPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityHeadingSameLevelPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityLandmarkPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityLinkPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityListPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityListItemPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityLiveRegionPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityMainPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityMediaPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityParagraphPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityRadioButtonPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityRadioGroupPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilitySectionPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityTablePredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityTextfieldPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityTextStyleBoldPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityTextStyleItalicPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityTextStyleUnderlinePredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityTreePredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityUnvisitedLinkPredicate);
DECLARE_ACCESSIBILITY_PREDICATE(AccessibilityVisitedLinkPredicate);

#undef DECLARE_ACCESSIBILITY_PREDICATE

// This class provides an interface for searching the accessibility tree from
// a given starting node, with a few built-in options and allowing an arbitrary
// number of predicates that can be used to restrict the search.
//
// This class is meant to perform one search. Initialize it, then iterate
// over the matches, and then delete it.
//
// This class stores raw pointers to the matches in the tree! Don't keep this
// object around if the tree is mutating.
class COMPONENT_EXPORT(AX_PLATFORM) OneShotAccessibilityTreeSearch {
 public:
  enum Direction { FORWARDS, BACKWARDS };

  const int UNLIMITED_RESULTS = -1;

  // The node passed in |scope| determines the scope of results returned -
  // they will all be within the subtree of the *parent* of |scope| - in other
  // words, siblings of |scope| and their descendants.
  explicit OneShotAccessibilityTreeSearch(BrowserAccessibility* scope);

  OneShotAccessibilityTreeSearch(const OneShotAccessibilityTreeSearch&) =
      delete;
  OneShotAccessibilityTreeSearch& operator=(
      const OneShotAccessibilityTreeSearch&) = delete;

  virtual ~OneShotAccessibilityTreeSearch();

  //
  // Search parameters.  All of these are optional.
  //

  // Sets the node where the search starts. The first potential match will
  // be the one immediately following this one. This node will be used as
  // the first arguement to any predicates.
  //
  // If not specified, |scope| will be used.
  void SetStartNode(BrowserAccessibility* start_node);

  // Search forwards or backwards in an in-order traversal of the tree.
  void SetDirection(Direction direction);

  // Set the maximum number of results, or UNLIMITED_RESULTS
  // for no limit (default).
  void SetResultLimit(int result_limit);

  // If true, only searches children of |start_node| and doesn't
  // recurse.
  void SetImmediateDescendantsOnly(bool immediate_descendants_only);

  // If true, wraps to the last element.
  void SetCanWrapToLastElement(bool can_wrap_to_last_element);

  // If true, only considers nodes that aren't offscreen.
  // Programmatically hidden elements are always skipped.
  void SetOnscreenOnly(bool onscreen_only);

  // Restricts the matches to only nodes where |text| is found as a
  // substring of any of that node's accessible text, including its
  // name, description, or value. Case-insensitive.
  void SetSearchText(const std::string& text);

  // Restricts the matches to only those that satisfy at least one of the
  // predicates.
  void AddPredicate(AccessibilityMatchPredicate predicate);

  //
  // Calling either of these executes the search.
  //

  size_t CountMatches();
  BrowserAccessibility* GetMatchAtIndex(size_t index);

 private:
  void Search();
  void SearchByWalkingTree();
  void SearchByIteratingOverChildren();
  bool Matches(BrowserAccessibility* node);

  raw_ptr<BrowserAccessibilityManager> tree_;
  raw_ptr<BrowserAccessibility> scope_node_;
  raw_ptr<BrowserAccessibility> start_node_;
  Direction direction_;
  int result_limit_;
  bool immediate_descendants_only_;
  bool can_wrap_to_last_element_;
  bool onscreen_only_;
  std::string search_text_;

  std::vector<AccessibilityMatchPredicate> predicates_;
  std::vector<raw_ptr<BrowserAccessibility, VectorExperimental>> matches_;

  bool did_search_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_ONE_SHOT_ACCESSIBILITY_TREE_SEARCH_H_
