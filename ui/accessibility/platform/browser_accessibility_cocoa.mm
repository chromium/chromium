// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/accessibility/platform/browser_accessibility_cocoa.h"

#include <execinfo.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#import "ui/accessibility/platform/ax_platform_node_mac.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/accessibility/platform/ax_utils_mac.h"
#include "ui/accessibility/platform/browser_accessibility_mac.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/browser_accessibility_manager_mac.h"
#include "ui/accessibility/platform/one_shot_accessibility_tree_search.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/strings/grit/ax_strings.h"

using AXPosition = ui::AXPlatformNodeDelegate::AXPosition;
using AXRange = ui::AXPlatformNodeDelegate::AXRange;
using StringAttribute = ax::mojom::StringAttribute;
using ui::AccessibilityMatchPredicate;
using ui::AXActionHandlerRegistry;
using ui::AXNodeData;
using ui::AXPlatformTreeManagerDelegate;
using ui::AXPositionToAXTextMarker;
using ui::AXRangeToAXTextMarkerRange;
using ui::AXTextMarkerRangeToAXRange;
using ui::AXTextMarkerToAXPosition;
using ui::BrowserAccessibility;
using ui::BrowserAccessibilityManager;
using ui::BrowserAccessibilityManagerMac;
using ui::OneShotAccessibilityTreeSearch;

static_assert(
    std::is_trivially_copyable<BrowserAccessibility::SerializedPosition>::value,
    "BrowserAccessibility::SerializedPosition must be POD because it's used to "
    "back an AXTextMarker");

namespace {

// Private WebKit accessibility attributes.
NSString* const
    NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute =
        @"AXUIElementCountForSearchPredicate";
NSString* const
    NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute =
        @"AXUIElementsForSearchPredicate";

// Private attributes for text markers.
NSString* const NSAccessibilityStartTextMarkerAttribute = @"AXStartTextMarker";
NSString* const NSAccessibilityEndTextMarkerAttribute = @"AXEndTextMarker";
NSString* const NSAccessibilitySelectedTextMarkerRangeAttribute =
    @"AXSelectedTextMarkerRange";
NSString* const NSAccessibilityTextMarkerIsValidParameterizedAttribute =
    @"AXTextMarkerIsValid";
NSString* const NSAccessibilityIndexForTextMarkerParameterizedAttribute =
    @"AXIndexForTextMarker";
NSString* const NSAccessibilityTextMarkerForIndexParameterizedAttribute =
    @"AXTextMarkerForIndex";
NSString* const NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute =
    @"AXEndTextMarkerForBounds";
NSString* const NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute =
    @"AXStartTextMarkerForBounds";
NSString* const
    NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXLineTextMarkerRangeForTextMarker";
// TODO(nektar): Implement programmatic text operations.
//
// NSString* const NSAccessibilityTextOperationMarkerRanges =
//    @"AXTextOperationMarkerRanges";
NSString* const NSAccessibilityUIElementForTextMarkerParameterizedAttribute =
    @"AXUIElementForTextMarker";
NSString* const
    NSAccessibilityTextMarkerRangeForUIElementParameterizedAttribute =
        @"AXTextMarkerRangeForUIElement";
NSString* const NSAccessibilityLineForTextMarkerParameterizedAttribute =
    @"AXLineForTextMarker";
NSString* const NSAccessibilityTextMarkerRangeForLineParameterizedAttribute =
    @"AXTextMarkerRangeForLine";
NSString* const NSAccessibilityStringForTextMarkerRangeParameterizedAttribute =
    @"AXStringForTextMarkerRange";
NSString* const NSAccessibilityTextMarkerForPositionParameterizedAttribute =
    @"AXTextMarkerForPosition";
NSString* const NSAccessibilityBoundsForTextMarkerRangeParameterizedAttribute =
    @"AXBoundsForTextMarkerRange";
NSString* const
    NSAccessibilityAttributedStringForTextMarkerRangeWithOptionsParameterizedAttribute =
        @"AXAttributedStringForTextMarkerRangeWithOptions";
NSString* const
    NSAccessibilityTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute =
        @"AXTextMarkerRangeForUnorderedTextMarkers";
NSString* const
    NSAccessibilityNextTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousTextMarkerForTextMarker";
NSString* const
    NSAccessibilityLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXLeftWordTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityRightWordTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXRightWordTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXLeftLineTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityRightLineTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXRightLineTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilitySentenceTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXSentenceTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityParagraphTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXParagraphTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityNextWordEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextWordEndTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousWordStartTextMarkerForTextMarker";
NSString* const
    NSAccessibilityNextLineEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextLineEndTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousLineStartTextMarkerForTextMarker";
NSString* const
    NSAccessibilityNextSentenceEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextSentenceEndTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousSentenceStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousSentenceStartTextMarkerForTextMarker";
NSString* const
    NSAccessibilityNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextParagraphEndTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousParagraphStartTextMarkerForTextMarker";
NSString* const
    NSAccessibilityStyleTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXStyleTextMarkerRangeForTextMarker";
NSString* const NSAccessibilityLengthForTextMarkerRangeParameterizedAttribute =
    @"AXLengthForTextMarkerRange";

// Private attributes that can be used for testing text markers, e.g. in dump
// tree tests.
NSString* const
    NSAccessibilityTextMarkerDebugDescriptionParameterizedAttribute =
        @"AXTextMarkerDebugDescription";
NSString* const
    NSAccessibilityTextMarkerRangeDebugDescriptionParameterizedAttribute =
        @"AXTextMarkerRangeDebugDescription";
NSString* const
    NSAccessibilityTextMarkerNodeDebugDescriptionParameterizedAttribute =
        @"AXTextMarkerNodeDebugDescription";

// Other private attributes.
NSString* const NSAccessibilitySelectTextWithCriteriaParameterizedAttribute =
    @"AXSelectTextWithCriteria";
NSString* const NSAccessibilityIndexForChildUIElementParameterizedAttribute =
    @"AXIndexForChildUIElement";
NSString* const NSAccessibilityValueAutofillAvailableAttribute =
    @"AXValueAutofillAvailable";
// Not currently supported by Chrome -- information not stored:
// NSString* const NSAccessibilityValueAutofilledAttribute =
// @"AXValueAutofilled";

// Not currently supported by Chrome -- mismatch of types supported: NSString*
// const NSAccessibilityValueAutofillTypeAttribute = @"AXValueAutofillType";

// Actions.
NSString* const NSAccessibilityScrollToVisibleAction = @"AXScrollToVisible";

// A mapping from an accessibility attribute to its method name.
NSDictionary* gAttributeToMethodNameMap = nil;

// VoiceOver uses -1 to mean "no limit" for AXResultsLimit.
const int kAXResultsLimitNoLimit = -1;

AXRange CreateAXRange(const BrowserAccessibility& start_object,
                      int start_offset,
                      ax::mojom::TextAffinity start_affinity,
                      const BrowserAccessibility& end_object,
                      int end_offset,
                      ax::mojom::TextAffinity end_affinity) {
  AXPosition anchor =
      start_object.CreatePositionAt(start_offset, start_affinity);
  AXPosition focus = end_object.CreatePositionAt(end_offset, end_affinity);
  // |AXRange| takes ownership of its anchor and focus.
  return AXRange(std::move(anchor), std::move(focus));
}

AXRange GetSelectedRange(BrowserAccessibility& owner) {
  const BrowserAccessibilityManager* manager = owner.manager();
  if (!manager)
    return {};

  const ui::AXSelection unignored_selection =
      manager->ax_tree()->GetUnignoredSelection();
  int32_t anchor_id = unignored_selection.anchor_object_id;
  const BrowserAccessibility* anchor_object = manager->GetFromID(anchor_id);
  if (!anchor_object)
    return {};

  int32_t focus_id = unignored_selection.focus_object_id;
  const BrowserAccessibility* focus_object = manager->GetFromID(focus_id);
  if (!focus_object)
    return {};

  // |anchor_offset| and / or |focus_offset| refer to a character offset if
  // |anchor_object| / |focus_object| are text-only objects or atomic text
  // fields. Otherwise, they should be treated as child indices. An atomic text
  // field does not expose its internal implementation to assistive software,
  // appearing as a single leaf node in the accessibility tree. It includes
  // <input>, <textarea> and Views-based text fields.
  int anchor_offset = unignored_selection.anchor_offset;
  int focus_offset = unignored_selection.focus_offset;
  DCHECK_GE(anchor_offset, 0);
  DCHECK_GE(focus_offset, 0);

  ax::mojom::TextAffinity anchor_affinity = unignored_selection.anchor_affinity;
  ax::mojom::TextAffinity focus_affinity = unignored_selection.focus_affinity;

  return CreateAXRange(*anchor_object, anchor_offset, anchor_affinity,
                       *focus_object, focus_offset, focus_affinity);
}

NSString* GetTextForTextMarkerRange(id marker_range) {
  AXRange range = AXTextMarkerRangeToAXRange(marker_range);
  if (range.IsNull())
    return nil;
  return base::SysUTF16ToNSString(range.GetText());
}

// GetState checks the bitmask used in AXNodeData to check
// if the given state was set on the accessibility object.
bool GetState(BrowserAccessibility* accessibility, ax::mojom::State state) {
  return accessibility->HasState(state);
}

// Given a search key provided to AXUIElementCountForSearchPredicate or
// AXUIElementsForSearchPredicate, return a predicate that can be added
// to OneShotAccessibilityTreeSearch.
AccessibilityMatchPredicate PredicateForSearchKey(NSString* searchKey) {
  if ([searchKey isEqualToString:@"AXAnyTypeSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return true;
    };
  } else if ([searchKey isEqualToString:@"AXBlockquoteSameLevelSearchKey"]) {
    // TODO(dmazzoni): implement the "same level" part.
    return ui::AccessibilityBlockquotePredicate;
  } else if ([searchKey isEqualToString:@"AXBlockquoteSearchKey"]) {
    return ui::AccessibilityBlockquotePredicate;
  } else if ([searchKey isEqualToString:@"AXBoldFontSearchKey"]) {
    return ui::AccessibilityTextStyleBoldPredicate;
  } else if ([searchKey isEqualToString:@"AXButtonSearchKey"]) {
    return ui::AccessibilityButtonPredicate;
  } else if ([searchKey isEqualToString:@"AXCheckBoxSearchKey"]) {
    return ui::AccessibilityCheckboxPredicate;
  } else if ([searchKey isEqualToString:@"AXControlSearchKey"]) {
    return ui::AccessibilityControlPredicate;
  } else if ([searchKey isEqualToString:@"AXDifferentTypeSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() != start->GetRole();
    };
  } else if ([searchKey isEqualToString:@"AXFontChangeSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXFontColorChangeSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXFrameSearchKey"]) {
    return ui::AccessibilityFramePredicate;
  } else if ([searchKey isEqualToString:@"AXGraphicSearchKey"]) {
    return ui::AccessibilityGraphicPredicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel1SearchKey"]) {
    return ui::AccessibilityH1Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel2SearchKey"]) {
    return ui::AccessibilityH2Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel3SearchKey"]) {
    return ui::AccessibilityH3Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel4SearchKey"]) {
    return ui::AccessibilityH4Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel5SearchKey"]) {
    return ui::AccessibilityH5Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel6SearchKey"]) {
    return ui::AccessibilityH6Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingSameLevelSearchKey"]) {
    return ui::AccessibilityHeadingSameLevelPredicate;
  } else if ([searchKey isEqualToString:@"AXHeadingSearchKey"]) {
    return ui::AccessibilityHeadingPredicate;
  } else if ([searchKey isEqualToString:@"AXHighlightedSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXItalicFontSearchKey"]) {
    return ui::AccessibilityTextStyleItalicPredicate;
  } else if ([searchKey isEqualToString:@"AXLandmarkSearchKey"]) {
    return ui::AccessibilityLandmarkPredicate;
  } else if ([searchKey isEqualToString:@"AXLinkSearchKey"]) {
    return ui::AccessibilityLinkPredicate;
  } else if ([searchKey isEqualToString:@"AXListSearchKey"]) {
    return ui::AccessibilityListPredicate;
  } else if ([searchKey isEqualToString:@"AXLiveRegionSearchKey"]) {
    return ui::AccessibilityLiveRegionPredicate;
  } else if ([searchKey isEqualToString:@"AXMisspelledWordSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXOutlineSearchKey"]) {
    return ui::AccessibilityTreePredicate;
  } else if ([searchKey isEqualToString:@"AXPlainTextSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXRadioGroupSearchKey"]) {
    return ui::AccessibilityRadioGroupPredicate;
  } else if ([searchKey isEqualToString:@"AXSameTypeSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() == start->GetRole();
    };
  } else if ([searchKey isEqualToString:@"AXStaticTextSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->IsText();
    };
  } else if ([searchKey isEqualToString:@"AXStyleChangeSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXTableSameLevelSearchKey"]) {
    // TODO(dmazzoni): implement the "same level" part.
    return ui::AccessibilityTablePredicate;
  } else if ([searchKey isEqualToString:@"AXTableSearchKey"]) {
    return ui::AccessibilityTablePredicate;
  } else if ([searchKey isEqualToString:@"AXTextFieldSearchKey"]) {
    return ui::AccessibilityTextfieldPredicate;
  } else if ([searchKey isEqualToString:@"AXUnderlineSearchKey"]) {
    return ui::AccessibilityTextStyleUnderlinePredicate;
  } else if ([searchKey isEqualToString:@"AXUnvisitedLinkSearchKey"]) {
    return ui::AccessibilityUnvisitedLinkPredicate;
  } else if ([searchKey isEqualToString:@"AXVisitedLinkSearchKey"]) {
    return ui::AccessibilityVisitedLinkPredicate;
  }

  return nullptr;
}

// Initialize a OneShotAccessibilityTreeSearch object given the parameters
// passed to AXUIElementCountForSearchPredicate or
// AXUIElementsForSearchPredicate. Return true on success.
bool InitializeAccessibilityTreeSearch(OneShotAccessibilityTreeSearch* search,
                                       id parameter) {
  if (![parameter isKindOfClass:[NSDictionary class]])
    return false;
  NSDictionary* dictionary = parameter;

  id startElementParameter = [dictionary objectForKey:@"AXStartElement"];
  if ([startElementParameter isKindOfClass:[BrowserAccessibilityCocoa class]]) {
    BrowserAccessibilityCocoa* startNodeCocoa =
        (BrowserAccessibilityCocoa*)startElementParameter;
    search->SetStartNode([startNodeCocoa owner]);
  }

  bool immediateDescendantsOnly = false;
  NSNumber* immediateDescendantsOnlyParameter =
      [dictionary objectForKey:@"AXImmediateDescendantsOnly"];
  if ([immediateDescendantsOnlyParameter isKindOfClass:[NSNumber class]])
    immediateDescendantsOnly = [immediateDescendantsOnlyParameter boolValue];

  bool onscreenOnly = false;
  // AXVisibleOnly actually means onscreen objects only -- nothing scrolled off.
  NSNumber* onscreenOnlyParameter = [dictionary objectForKey:@"AXVisibleOnly"];
  if ([onscreenOnlyParameter isKindOfClass:[NSNumber class]])
    onscreenOnly = [onscreenOnlyParameter boolValue];

  ui::OneShotAccessibilityTreeSearch::Direction direction =
      ui::OneShotAccessibilityTreeSearch::FORWARDS;
  NSString* directionParameter = [dictionary objectForKey:@"AXDirection"];
  if ([directionParameter isKindOfClass:[NSString class]]) {
    if ([directionParameter isEqualToString:@"AXDirectionNext"])
      direction = ui::OneShotAccessibilityTreeSearch::FORWARDS;
    else if ([directionParameter isEqualToString:@"AXDirectionPrevious"])
      direction = ui::OneShotAccessibilityTreeSearch::BACKWARDS;
  }

  int resultsLimit = kAXResultsLimitNoLimit;
  NSNumber* resultsLimitParameter = [dictionary objectForKey:@"AXResultsLimit"];
  if ([resultsLimitParameter isKindOfClass:[NSNumber class]])
    resultsLimit = [resultsLimitParameter intValue];

  std::string searchText;
  NSString* searchTextParameter = [dictionary objectForKey:@"AXSearchText"];
  if ([searchTextParameter isKindOfClass:[NSString class]])
    searchText = base::SysNSStringToUTF8(searchTextParameter);

  search->SetDirection(direction);
  search->SetImmediateDescendantsOnly(immediateDescendantsOnly);
  search->SetOnscreenOnly(onscreenOnly);
  search->SetSearchText(searchText);

  // Mac uses resultsLimit == -1 for unlimited, that that's
  // the default for OneShotAccessibilityTreeSearch already.
  // Only set the results limit if it's nonnegative.
  if (resultsLimit >= 0)
    search->SetResultLimit(resultsLimit);

  id searchKey = [dictionary objectForKey:@"AXSearchKey"];
  if ([searchKey isKindOfClass:[NSString class]]) {
    AccessibilityMatchPredicate predicate =
        PredicateForSearchKey((NSString*)searchKey);
    if (predicate)
      search->AddPredicate(predicate);
  } else if ([searchKey isKindOfClass:[NSArray class]]) {
    size_t searchKeyCount = static_cast<size_t>([searchKey count]);
    for (size_t i = 0; i < searchKeyCount; ++i) {
      id key = [searchKey objectAtIndex:i];
      if ([key isKindOfClass:[NSString class]]) {
        AccessibilityMatchPredicate predicate =
            PredicateForSearchKey((NSString*)key);
        if (predicate)
          search->AddPredicate(predicate);
      }
    }
  }

  return true;
}

bool IsSelectedStateRelevant(BrowserAccessibility* item) {
  if (!item->HasBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    return false;  // Does not have selected state -> not relevant.

  BrowserAccessibility* container = item->PlatformGetSelectionContainer();
  if (!container)
    return false;  // No container -> not relevant.

  if (container->HasState(ax::mojom::State::kMultiselectable))
    return true;  // In a multiselectable -> is relevant.

  // Single selection AND not selected - > is relevant.
  // Single selection containers can explicitly set the focused item as not
  // selected, for example via aria-selectable="false". It's useful for the user
  // to know that it's not selected in this case.
  // Only do this for the focused item -- that is the only item where explicitly
  // setting the item to unselected is relevant, as the focused item is the only
  // item that could have been selected anyway.
  // Therefore, if the user navigates to other items by detaching accessibility
  // focus from the input focus via VO+Shift+F3, those items will not be
  // redundantly reported as not selected.
  return item->manager()->GetFocus() == item &&
         !item->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

}  // namespace

namespace ui {

AXTextEdit::AXTextEdit() = default;
AXTextEdit::AXTextEdit(std::u16string inserted_text,
                       std::u16string deleted_text,
                       id edit_text_marker)
    : inserted_text(inserted_text),
      deleted_text(deleted_text),
      edit_text_marker(edit_text_marker) {}
AXTextEdit::AXTextEdit(const AXTextEdit& other) = default;
AXTextEdit::~AXTextEdit() = default;

}  // namespace ui

// Not defined in current versions of library, but may be in the future:
#ifndef NSAccessibilityLanguageAttribute
#define NSAccessibilityLanguageAttribute @"AXLanguage"
#endif

bool ui::IsNSRange(id value) {
  return [value isKindOfClass:[NSValue class]] &&
         0 == strcmp([value objCType], @encode(NSRange));
}

@implementation BrowserAccessibilityCocoa {
  // Dangling pointer https://crbug.com/1475830.
  raw_ptr<ui::BrowserAccessibility, DanglingUntriaged> _owner;
  // An array of children of this object. Cached to avoid re-computing.
  NSMutableArray* __strong _children;
  // Whether the children have changed and need to be updated.
  bool _needsToUpdateChildren;
  // Whether _children is currently being computed.
  bool _gettingChildren;
  // Stores the previous value of an edit field.
  std::u16string _oldValue;
}

+ (void)initialize {
  const struct {
    NSString* attribute;
    NSString* methodName;
  } attributeToMethodNameContainer[] = {
      {NSAccessibilityChildrenAttribute, @"children"},
      {NSAccessibilityColumnsAttribute, @"columns"},
      {NSAccessibilityColumnIndexRangeAttribute, @"columnIndexRange"},
      {NSAccessibilityContentsAttribute, @"contents"},
      {NSAccessibilityDisclosingAttribute, @"disclosing"},
      {NSAccessibilityDisclosedByRowAttribute, @"disclosedByRow"},
      {NSAccessibilityDisclosureLevelAttribute, @"disclosureLevel"},
      {NSAccessibilityDisclosedRowsAttribute, @"disclosedRows"},
      {NSAccessibilityEnabledAttribute, @"enabled"},
      {NSAccessibilityEndTextMarkerAttribute, @"endTextMarker"},
      {NSAccessibilityExpandedAttribute, @"expanded"},
      {NSAccessibilityHeaderAttribute, @"header"},
      {NSAccessibilityIndexAttribute, @"index"},
      {NSAccessibilityInsertionPointLineNumberAttribute,
       @"insertionPointLineNumber"},
      {NSAccessibilityLanguageAttribute, @"language"},
      {NSAccessibilityLinkedUIElementsAttribute, @"linkedUIElements"},
      {NSAccessibilityMaxValueAttribute, @"maxValue"},
      {NSAccessibilityMinValueAttribute, @"minValue"},
      {NSAccessibilityNumberOfCharactersAttribute, @"numberOfCharacters"},
      {NSAccessibilityOrientationAttribute, @"orientation"},
      {NSAccessibilityParentAttribute, @"parent"},
      {NSAccessibilityPositionAttribute, @"position"},
      {NSAccessibilityRoleAttribute, @"role"},
      {NSAccessibilityRowHeaderUIElementsAttribute, @"rowHeaders"},
      {NSAccessibilityRowIndexRangeAttribute, @"rowIndexRange"},
      {NSAccessibilityRowsAttribute, @"accessibilityRows"},
      // TODO(aboxhall): expose
      // NSAccessibilityServesAsTitleForUIElementsAttribute
      {NSAccessibilityStartTextMarkerAttribute, @"startTextMarker"},
      {NSAccessibilitySelectedChildrenAttribute, @"selectedChildren"},
      {NSAccessibilitySelectedTextAttribute, @"selectedText"},
      {NSAccessibilitySelectedTextRangeAttribute, @"selectedTextRange"},
      {NSAccessibilitySelectedTextMarkerRangeAttribute,
       @"selectedTextMarkerRange"},
      {NSAccessibilitySortDirectionAttribute, @"sortDirection"},
      {NSAccessibilitySubroleAttribute, @"subrole"},
      {NSAccessibilityTabsAttribute, @"tabs"},
      {NSAccessibilityTopLevelUIElementAttribute, @"window"},
      {NSAccessibilityValueAttribute, @"value"},
      {NSAccessibilityValueAutofillAvailableAttribute,
       @"valueAutofillAvailable"},
      // Not currently supported by Chrome -- information not stored:
      // {NSAccessibilityValueAutofilledAttribute, @"valueAutofilled"},
      // Not currently supported by Chrome -- mismatch of types supported:
      // {NSAccessibilityValueAutofillTypeAttribute, @"valueAutofillType"},
      {NSAccessibilityValueDescriptionAttribute, @"valueDescription"},
      {NSAccessibilityVisibleCharacterRangeAttribute, @"visibleCharacterRange"},
      {NSAccessibilityVisibleCellsAttribute, @"visibleCells"},
      {NSAccessibilityVisibleChildrenAttribute, @"visibleChildren"},
      {NSAccessibilityVisibleColumnsAttribute, @"visibleColumns"},
      {NSAccessibilityVisibleRowsAttribute, @"visibleRows"},
      {NSAccessibilityWindowAttribute, @"window"},
  };

  NSMutableDictionary* dict = [[NSMutableDictionary alloc] init];
  for (const auto& item : attributeToMethodNameContainer) {
    dict[item.attribute] = item.methodName;
  }
  gAttributeToMethodNameMap = dict;
}

- (instancetype)initWithObject:(BrowserAccessibility*)accessibility
              withPlatformNode:(ui::AXPlatformNodeMac*)platform_node {
  if ((self = [super initWithNode:platform_node])) {
    _owner = accessibility;
    _needsToUpdateChildren = true;
    _gettingChildren = false;
  }
  return self;
}

- (void)detach {
  if (!_owner)
    return;

  _owner = nullptr;
  [super detach];
}

- (NSArray*)AXChildren {
  return [self children];
}

// Returns an array of BrowserAccessibilityCocoa objects, representing the
// accessibility children of this object.
- (NSArray*)children {
  if (![self instanceActive])
    return nil;
  if (_needsToUpdateChildren) {
    base::AutoReset<bool> set_getting_children(&_gettingChildren, true);
    // PlatformChildCount may add extra mac nodes if the node requires them.
    uint32_t childCount = _owner->PlatformChildCount();
    _children = [[NSMutableArray alloc] initWithCapacity:childCount];
    for (auto it = _owner->PlatformChildrenBegin();
         it != _owner->PlatformChildrenEnd(); ++it) {
      AXPlatformNodeCocoa* child = it->GetNativeViewAccessible();
      if ([child isIncludedInPlatformTree])
        [_children addObject:child];
      else
        [_children addObjectsFromArray:[child accessibilityChildren]];
    }

    // Also, add indirect children (if any).
    const std::vector<int32_t>& indirectChildIds = _owner->GetIntListAttribute(
        ax::mojom::IntListAttribute::kIndirectChildIds);
    for (uint32_t i = 0; i < indirectChildIds.size(); ++i) {
      int32_t child_id = indirectChildIds[i];
      BrowserAccessibility* child = _owner->manager()->GetFromID(child_id);

      // This only became necessary as a result of crbug.com/93095. It should be
      // a DCHECK in the future.
      if (child)
        [_children addObject:child->GetNativeViewAccessible()];
    }
    _needsToUpdateChildren = false;
  }
  return _children;
}

- (void)childrenChanged {
  // This function may be called in the middle of children() if this node adds
  // extra mac nodes while its children are being requested. If _gettingChildren
  // is true, we don't need to do anything here.
  if (![self instanceActive] || _gettingChildren)
    return;
  _needsToUpdateChildren = true;
  if (![self isIncludedInPlatformTree]) {
    BrowserAccessibility* parent = _owner->PlatformGetParent();
    if (parent)
      [parent->GetNativeViewAccessible() childrenChanged];
  }
}

- (NSValue*)columnIndexRange {
  if (![self instanceActive])
    return nil;

  std::optional<int> column = _owner->node()->GetTableCellColIndex();
  std::optional<int> colspan = _owner->node()->GetTableCellColSpan();
  if (column && colspan)
    return [NSValue valueWithRange:NSMakeRange(*column, *colspan)];
  return nil;
}

- (NSArray*)columns {
  if (![self instanceActive])
    return nil;
  NSMutableArray* ret = [[NSMutableArray alloc] init];
  for (AXPlatformNodeCocoa* child in [self accessibilityChildren]) {
    if ([[child accessibilityRole] isEqualToString:NSAccessibilityColumnRole])
      [ret addObject:child];
  }
  return ret;
}

- (BrowserAccessibility*)containingTable {
  BrowserAccessibility* table = _owner;
  while (table && !ui::IsTableLike(table->GetRole())) {
    table = table->PlatformGetParent();
  }
  return table;
}

- (NSNumber*)disclosing {
  return @([self isAccessibilityDisclosed]);
}

- (id)disclosedByRow {
  return [self accessibilityDisclosedByRow];
}

- (NSNumber*)disclosureLevel {
  return [NSNumber numberWithInteger:[self accessibilityDisclosureLevel]];
}

- (id)disclosedRows {
  return [self accessibilityDisclosedRows];
}

- (NSNumber*)enabled {
  if (![self instanceActive])
    return nil;
  return @(_owner->GetData().GetRestriction() !=
           ax::mojom::Restriction::kDisabled);
}

// Returns a text marker that points to the last character in the document that
// can be selected with VoiceOver.
- (id)endTextMarker {
  if (![self instanceActive])
    return nil;
  AXPosition position = _owner->CreateTextPositionAt(0);
  return AXPositionToAXTextMarker(position->CreatePositionAtEndOfContent());
}

- (NSNumber*)expanded {
  if (![self instanceActive])
    return nil;
  return @(GetState(_owner, ax::mojom::State::kExpanded));
}

- (BOOL)isAccessibilityFocused {
  if (![self instanceActive])
    return NO;

  BrowserAccessibilityManager* manager = _owner->manager();
  return manager->GetFocus() == _owner;
}

- (void)setAccessibilityFocused:(BOOL)flag {
  BrowserAccessibilityManager* manager = _owner->manager();
  if (flag) {
    manager->SetFocus(*_owner);
  }
}

- (id)header {
  if (![self instanceActive])
    return nil;
  int headerElementId = -1;
  if (ui::IsTableLike(_owner->GetRole())) {
    // The table header container is always the last child of the table,
    // if it exists. The table header container is a special node in the
    // accessibility tree only used on macOS. It has all of the table
    // headers as its children, even though those cells are also children
    // of rows in the table. Internally this is implemented using
    // AXTableInfo and indirect_child_ids.
    uint32_t childCount = _owner->PlatformChildCount();
    if (childCount > 0) {
      BrowserAccessibility* tableHeader = _owner->PlatformGetLastChild();
      if (tableHeader->GetRole() == ax::mojom::Role::kTableHeaderContainer)
        return tableHeader->GetNativeViewAccessible();
    }
  } else if ([self internalRole] == ax::mojom::Role::kColumn) {
    _owner->GetIntAttribute(ax::mojom::IntAttribute::kTableColumnHeaderId,
                            &headerElementId);
  } else if ([self internalRole] == ax::mojom::Role::kRow) {
    _owner->GetIntAttribute(ax::mojom::IntAttribute::kTableRowHeaderId,
                            &headerElementId);
  }

  if (headerElementId > 0) {
    BrowserAccessibility* headerObject =
        _owner->manager()->GetFromID(headerElementId);
    if (headerObject)
      return headerObject->GetNativeViewAccessible();
  }
  return nil;
}

- (NSNumber*)index {
  if (![self instanceActive])
    return nil;

  if ([self internalRole] == ax::mojom::Role::kTreeItem) {
    return [self treeItemRowIndex];
  } else if ([self internalRole] == ax::mojom::Role::kColumn) {
    DCHECK(_owner->node());
    std::optional<int> col_index = *_owner->node()->GetTableColColIndex();
    if (col_index)
      return @(*col_index);
  } else if ([self internalRole] == ax::mojom::Role::kRow) {
    DCHECK(_owner->node());
    std::optional<int> row_index = _owner->node()->GetTableRowRowIndex();
    if (row_index)
      return @(*row_index);
  }

  return nil;
}

- (NSNumber*)treeItemRowIndex {
  if (![self instanceActive])
    return nil;

  DCHECK([self internalRole] == ax::mojom::Role::kTreeItem);
  DCHECK([[self role] isEqualToString:NSAccessibilityRowRole]);

  // First find an ancestor that establishes this tree or treegrid. We
  // will search in this ancestor to calculate our row index.
  BrowserAccessibility* container = [self owner]->PlatformGetParent();
  while (container && container->GetRole() != ax::mojom::Role::kTree &&
         container->GetRole() != ax::mojom::Role::kTreeGrid) {
    container = container->PlatformGetParent();
  }
  if (!container)
    return nil;

  const BrowserAccessibilityCocoa* cocoaContainer =
      container->GetNativeViewAccessible();
  int currentIndex = 0;
  if ([cocoaContainer findRowIndex:self withCurrentIndex:&currentIndex]) {
    return @(currentIndex);
  }

  return nil;
}

- (bool)findRowIndex:(BrowserAccessibilityCocoa*)toFind
    withCurrentIndex:(int*)currentIndex {
  if (![self instanceActive])
    return false;

  DCHECK([[toFind accessibilityRole] isEqualToString:NSAccessibilityRowRole]);
  for (BrowserAccessibilityCocoa* childToCheck in
       [self accessibilityChildren]) {
    if ([toFind isEqual:childToCheck]) {
      return true;
    }

    if ([[childToCheck accessibilityRole]
            isEqualToString:NSAccessibilityRowRole]) {
      ++(*currentIndex);
    }

    if ([childToCheck findRowIndex:toFind withCurrentIndex:currentIndex]) {
      return true;
    }
  }

  return false;
}

- (NSNumber*)AXInsertionPointLineNumber {
  return [self insertionPointLineNumber];
}

- (NSNumber*)insertionPointLineNumber {
  if (![self instanceActive])
    return nil;
  if (!_owner->HasVisibleCaretOrSelection())
    return nil;

  const AXRange range = GetSelectedRange(*_owner);

  // If the selection is not collapsed, then there is no visible caret.
  if (!range.IsCollapsed())
    return nil;

  // "ax::mojom::MoveDirection" is only relevant on platforms that use object
  // replacement characters in the accessibility tree. Mac is not one of them.
  const AXPosition caretPosition = range.focus()->LowestCommonAncestorPosition(
      *_owner->CreateTextPositionAt(0), ax::mojom::MoveDirection::kForward);
  DCHECK(!caretPosition->IsNullPosition())
      << "Calling HasVisibleCaretOrSelection() should have ensured that there "
         "is a valid selection focus inside the current object.";
  const std::vector<int> lineStarts =
      _owner->GetIntListAttribute(ax::mojom::IntListAttribute::kLineStarts);

  // Find the text offset that starts the next line after the current caret
  // position, then subtract 1 to get the current line number.
  auto iterator =
      std::upper_bound(lineStarts.begin(), lineStarts.end(),
                       caretPosition->AsTextPosition()->text_offset());

  // If the caret is on a single line and the line is empty, then
  // the iterator will be equal to lineStarts.begin() because the lineStarts
  // vector will be empty. The line number should be 0 in this case.
  if (iterator == lineStarts.begin())
    return @(0);

  return @(std::distance(lineStarts.begin(), std::prev(iterator)));
}

- (NSString*)language {
  if (![self instanceActive])
    return nil;
  ui::AXNode* node = _owner->node();
  DCHECK(node);
  return base::SysUTF8ToNSString(node->GetLanguage());
}

// private
- (void)addLinkedUIElementsFromAttribute:(ax::mojom::IntListAttribute)attribute
                                   addTo:(NSMutableArray*)outArray {
  const std::vector<int32_t>& attributeValues =
      _owner->GetIntListAttribute(attribute);
  for (size_t i = 0; i < attributeValues.size(); ++i) {
    BrowserAccessibility* element =
        _owner->manager()->GetFromID(attributeValues[i]);
    if (element)
      [outArray addObject:element->GetNativeViewAccessible()];
  }
}

// private
- (NSArray*)linkedUIElements {
  NSMutableArray* ret = [[NSMutableArray alloc] init];
  [self
      addLinkedUIElementsFromAttribute:ax::mojom::IntListAttribute::kControlsIds
                                 addTo:ret];
  [self addLinkedUIElementsFromAttribute:ax::mojom::IntListAttribute::kFlowtoIds
                                   addTo:ret];

  int target_id;
  if (_owner->GetIntAttribute(ax::mojom::IntAttribute::kInPageLinkTargetId,
                              &target_id)) {
    BrowserAccessibility* target =
        _owner->manager()->GetFromID(static_cast<int32_t>(target_id));
    if (target)
      [ret addObject:target->GetNativeViewAccessible()];
  }

  [self addLinkedUIElementsFromAttribute:ax::mojom::IntListAttribute::
                                             kRadioGroupIds
                                   addTo:ret];
  return ret;
}

- (NSNumber*)maxValue {
  if (![self instanceActive])
    return nil;

  if (!_owner->HasFloatAttribute(ax::mojom::FloatAttribute::kValueForRange))
    return @0;  // Indeterminate value exposes AXMinValue/AXMaxValue of 0.

  float floatValue =
      _owner->GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
  return @(floatValue);
}

- (NSNumber*)minValue {
  if (![self instanceActive])
    return nil;

  if (!_owner->HasFloatAttribute(ax::mojom::FloatAttribute::kValueForRange))
    return @0;  // Indeterminate value exposes AXMinValue/AXMaxValue of 0.

  float floatValue =
      _owner->GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
  return @(floatValue);
}

- (NSString*)orientation {
  if (![self instanceActive])
    return nil;
  if (GetState(_owner, ax::mojom::State::kVertical))
    return NSAccessibilityVerticalOrientationValue;
  else if (GetState(_owner, ax::mojom::State::kHorizontal))
    return NSAccessibilityHorizontalOrientationValue;

  return @"";
}

- (NSNumber*)AXNumberOfCharacters {
  return [self numberOfCharacters];
}

- (NSNumber*)numberOfCharacters {
  if ([self instanceActive] && _owner->IsTextField())
    return @(static_cast<int>(_owner->GetValueForControl().size()));
  return nil;
}

- (id)parent {
  if (![self instanceActive])
    return nil;
  if (_owner->PlatformGetParent()) {
    id unignored_parent = NSAccessibilityUnignoredAncestor(
        _owner->PlatformGetParent()->GetNativeViewAccessible());
    DCHECK(unignored_parent);
    return unignored_parent;
  }

  // A nil parent means we're the root.
  // Hook back up to RenderWidgetHostViewCocoa.
  BrowserAccessibilityManagerMac* manager =
      _owner->manager()
          ->GetManagerForRootFrame()
          ->ToBrowserAccessibilityManagerMac();
  if (!manager) {
    // TODO(accessibility) Determine why this is happening.
    DCHECK(false);
    return nil;
  }
  SANITIZER_CHECK(manager->GetParentView());
  return manager->GetParentView();
}

- (NSValue*)position {
  if (![self instanceActive])
    return nil;
  NSPoint pointInScreen = [self accessibilityFrame].origin;
  return [NSValue valueWithPoint:pointInScreen];
}

- (ui::BrowserAccessibility*)owner {
  return _owner;
}

// Assumes that there is at most one insertion, deletion or replacement at once.
// TODO(nektar): Merge this method with
// |BrowserAccessibilityAndroid::CommonEndLengths|.
- (ui::AXTextEdit)computeTextEdit {
  if (!_owner->IsTextField())
    return ui::AXTextEdit();

  // Starting from macOS 10.11, if the user has edited some text we need to
  // dispatch the actual text that changed on the value changed notification.
  // We run this code on all macOS versions to get the highest test coverage.
  std::u16string oldValue = _oldValue;
  std::u16string newValue = _owner->CreateTextPositionAt(0)->GetText(
      ui::AXEmbeddedObjectBehavior::kSuppressCharacter);
  _oldValue = newValue;
  if (oldValue.empty() && newValue.empty())
    return ui::AXTextEdit();

  size_t i;
  size_t j;
  // Sometimes Blink doesn't use the same UTF16 characters to represent
  // whitespace.
  for (i = 0;
       i < oldValue.length() && i < newValue.length() &&
       (oldValue[i] == newValue[i] || (base::IsUnicodeWhitespace(oldValue[i]) &&
                                       base::IsUnicodeWhitespace(newValue[i])));
       ++i) {
  }
  for (j = 0;
       (i + j) < oldValue.length() && (i + j) < newValue.length() &&
       (oldValue[oldValue.length() - j - 1] ==
            newValue[newValue.length() - j - 1] ||
        (base::IsUnicodeWhitespace(oldValue[oldValue.length() - j - 1]) &&
         base::IsUnicodeWhitespace(newValue[newValue.length() - j - 1])));
       ++j) {
  }
  DCHECK_LE(i + j, oldValue.length());
  DCHECK_LE(i + j, newValue.length());

  std::u16string deletedText = oldValue.substr(i, oldValue.length() - i - j);
  std::u16string insertedText = newValue.substr(i, newValue.length() - i - j);

  // Heuristic for editable combobox. If more than 1 character is inserted or
  // deleted, and the caret is at the end of the field, assume the entire text
  // field changed.
  // TODO(nektar) Remove this once editing intents are implemented,
  // and the actual inserted and deleted text is passed over from Blink.
  if ([self internalRole] == ax::mojom::Role::kTextFieldWithComboBox &&
      (deletedText.length() > 1 || insertedText.length() > 1)) {
    int sel_start, sel_end;
    _owner->GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart, &sel_start);
    _owner->GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, &sel_end);
    if (static_cast<size_t>(sel_start) == newValue.length() &&
        static_cast<size_t>(sel_end) == newValue.length()) {
      // Don't include oldValue as it would be announced -- very confusing.
      return ui::AXTextEdit(newValue, std::u16string(), nil);
    }
  }
  return ui::AXTextEdit(
      insertedText, deletedText,
      AXPositionToAXTextMarker(_owner->CreateTextPositionAt(i)));
}

// internal
- (NSRect)rectInScreen:(gfx::Rect)layout_rect {
  if (![self instanceActive])
    return NSZeroRect;

  auto rect = ScaleToRoundedRect(
      layout_rect, 1.f / _owner->manager()->device_scale_factor());

  // Get the delegate for the topmost BrowserAccessibilityManager, because
  // that's the only one that can convert points to their origin in the screen.
  ui::AXPlatformTreeManagerDelegate* delegate =
      _owner->manager()->GetDelegateFromRootManager();
  if (delegate) {
    return gfx::ScreenRectToNSRect(
        rect + delegate->AccessibilityGetViewBounds().OffsetFromOrigin());
  } else {
    return NSZeroRect;
  }
}

// Returns a string indicating the NSAccessibility role of this object.
- (NSString*)AXRole {
  return [self role];
}
- (NSString*)role {
  if (![self instanceActive]) {
    TRACE_EVENT0("accessibility", "BrowserAccessibilityCocoa::role nil");
    return nil;
  }

  NSString* cocoa_role = nil;
  ax::mojom::Role role = [self internalRole];
  if (role == ax::mojom::Role::kCanvas &&
      _owner->GetBoolAttribute(ax::mojom::BoolAttribute::kCanvasHasFallback)) {
    cocoa_role = NSAccessibilityGroupRole;
  } else if (_owner->IsTextField() &&
             _owner->HasState(ax::mojom::State::kMultiline) &&
             !_owner->GetData().IsSpinnerTextField()) {
    cocoa_role = NSAccessibilityTextAreaRole;
  } else if (ui::IsImage(_owner->GetRole()) && _owner->GetChildCount()) {
    // An image map is an image with children, and exposed on Mac as a group.
    cocoa_role = NSAccessibilityGroupRole;
  } else if (ui::IsImage(_owner->GetRole()) &&
             _owner->HasExplicitlyEmptyName()) {
    cocoa_role = NSAccessibilityUnknownRole;
  } else if (_owner->IsRootWebAreaForPresentationalIframe()) {
    cocoa_role = NSAccessibilityGroupRole;
  } else {
    cocoa_role = [AXPlatformNodeCocoa nativeRoleFromAXRole:role];
  }

  TRACE_EVENT1("accessibility", "BrowserAccessibilityCocoa::role",
               "role=", base::SysNSStringToUTF8(cocoa_role));
  return cocoa_role;
}

- (NSArray*)rowHeaders {
  if (![self instanceActive]) {
    return nil;
  }

  bool isCellOrTableHeader = ui::IsCellOrTableHeader(_owner->GetRole());
  bool isTableLike = ui::IsTableLike(_owner->GetRole());
  if (!isTableLike && !isCellOrTableHeader) {
    return nil;
  }

  BrowserAccessibility* table = [self containingTable];
  if (!table) {
    return nil;
  }

  // A table with no row headers.
  if (isTableLike && !table->GetTableRowCount().has_value()) {
    return nil;
  }

  NSMutableArray* rowHeaders = [[NSMutableArray alloc] init];

  if (isTableLike) {
    // Return the table's row headers.
    std::set<int32_t> headerIds;

    int numberOfRows = table->GetTableRowCount().value();

    // Rows can have more than one row header cell. Also, we apparently need
    // to guard against duplicate row header ids. Storing in a set dedups.
    for (int i = 0; i < numberOfRows; i++) {
      std::vector<int32_t> rowHeaderIds = table->GetRowHeaderNodeIds(i);
      for (int32_t rowHeaderId : rowHeaderIds) {
        headerIds.insert(rowHeaderId);
      }
    }

    for (int32_t headerId : headerIds) {
      BrowserAccessibility* cell = _owner->manager()->GetFromID(headerId);
      if (cell) {
        [rowHeaders addObject:cell->GetNativeViewAccessible()];
      }
    }
  } else {
    // Otherwise this is a cell, return the row headers for this cell.
    for (int32_t nodeId : _owner->node()->GetTableCellRowHeaderNodeIds()) {
      BrowserAccessibility* cell = _owner->manager()->GetFromID(nodeId);
      if (cell) {
        [rowHeaders addObject:cell->GetNativeViewAccessible()];
      }
    }
  }

  return [rowHeaders count] ? rowHeaders : nil;
}

- (NSValue*)rowIndexRange {
  if (![self instanceActive])
    return nil;

  std::optional<int> row = _owner->node()->GetTableCellRowIndex();
  std::optional<int> rowspan = _owner->node()->GetTableCellRowSpan();
  if (row && rowspan)
    return [NSValue valueWithRange:NSMakeRange(*row, *rowspan)];
  return nil;
}

- (NSArray*)accessibilityRows {
  if (![self instanceActive])
    return nil;
  NSMutableArray* ret = [[NSMutableArray alloc] init];

  std::vector<int32_t> node_id_list;
  if (_owner->GetRole() == ax::mojom::Role::kTree)
    [self getTreeItemDescendantNodeIds:&node_id_list];
  else if (ui::IsTableLike(_owner->GetRole()))
    node_id_list = _owner->node()->GetTableRowNodeIds();
  // Rows attribute for a column is the list of all the elements in that column
  // at each row.
  else if ([self internalRole] == ax::mojom::Role::kColumn)
    node_id_list = _owner->GetIntListAttribute(
        ax::mojom::IntListAttribute::kIndirectChildIds);

  for (int32_t node_id : node_id_list) {
    BrowserAccessibility* rowElement = _owner->manager()->GetFromID(node_id);
    if (rowElement)
      [ret addObject:rowElement->GetNativeViewAccessible()];
  }

  return ret;
}

- (NSArray*)selectedChildren {
  if (![self instanceActive])
    return nil;

  NSMutableArray* ret = [[NSMutableArray alloc] init];
  BrowserAccessibility* focusedChild = _owner->manager()->GetFocus();

  // "IsDescendantOf" also returns true when the two objects are equivalent.
  if (focusedChild && focusedChild != _owner &&
      focusedChild->IsDescendantOf(_owner)) {
    // If this container is not multi-selectable, try to skip iterating over the
    // children because there could only be at most one selected child. The
    // selected child should also be equivalent to the focused child, because
    // selection is tethered to the focus.
    if (!GetState(_owner, ax::mojom::State::kMultiselectable) &&
        focusedChild->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
      [ret addObject:focusedChild->GetNativeViewAccessible()];
      return ret;
    }

    // If this container is multi-selectable, the focused child should be
    // the first item in the list of selected children regardless of whether
    // it is selected or not, because this is how VoiceOver determines where to
    // draw the focus ring around the active item.
    //
    // Not appending this item when focused but not selected would result in
    // VoiceOver's focus ring jumping to the first selected item. It's unclear
    // if this is by design or not, but VoiceOver folks confirmed offline that
    // Safari always append the focused item, whether selected or not, to the
    // list of selected items.
    if (GetState(_owner, ax::mojom::State::kMultiselectable))
      [ret addObject:focusedChild->GetNativeViewAccessible()];
  }

  // If this container is multi-selectable, we need to return any additional
  // children (other than the focused child) with the "selected" state. If this
  // container is not multi-selectable, but none of its children have the focus,
  // we need to return all its children with the "selected" state.
  for (auto it = _owner->PlatformChildrenBegin();
       it != _owner->PlatformChildrenEnd(); ++it) {
    BrowserAccessibility* child = it.get();
    if (child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected) &&
        child != focusedChild) {
      [ret addObject:child->GetNativeViewAccessible()];
    }
  }

  return ret;
}

- (NSString*)AXSelectedText {
  return [self selectedText];
}

- (NSString*)selectedText {
  if (![self instanceActive])
    return nil;
  if (!_owner->HasVisibleCaretOrSelection())
    return nil;

  const AXRange range = GetSelectedRange(*_owner);
  if (range.IsNull())
    return nil;
  return base::SysUTF16ToNSString(range.GetText());
}

- (NSValue*)AXSelectedTextRange {
  return [self selectedTextRange];
}

// Returns range of text under the current object that is selected.
//
// Example, caret at offset 5:
// NSRange:  “pos=5 len=0”
- (NSValue*)selectedTextRange {
  if (![self instanceActive])
    return nil;
  if (!_owner->HasVisibleCaretOrSelection())
    return nil;

  const AXRange range = GetSelectedRange(*_owner).AsForwardRange();
  if (range.IsNull())
    return nil;

  // "ax::mojom::MoveDirection" is only relevant on platforms that use object
  // replacement characters in the accessibility tree. Mac is not one of them.
  const AXPosition startPosition = range.anchor()->LowestCommonAncestorPosition(
      *_owner->CreateTextPositionAt(0), ax::mojom::MoveDirection::kForward);
  DCHECK(!startPosition->IsNullPosition())
      << "Calling HasVisibleCaretOrSelection() should have ensured that there "
         "is a valid selection anchor inside the current object.";
  int selStart = startPosition->AsTextPosition()->text_offset();
  DCHECK_GE(selStart, 0);
  int selLength = range.GetText().length();
  return [NSValue valueWithRange:NSMakeRange(selStart, selLength)];
}

- (void)setAccessibilitySelectedTextRange:(NSRange)range {
  if (![self instanceActive])
    return;

  BrowserAccessibilityManager* manager = _owner->manager();
  manager->SetSelection(BrowserAccessibility::AXRange(
      _owner->CreateTextPositionAt(range.location)->AsDomSelectionPosition(),
      _owner->CreateTextPositionAt(NSMaxRange(range))
          ->AsDomSelectionPosition()));
}

- (id)selectedTextMarkerRange {
  if (![self instanceActive])
    return nil;
  AXRange ax_range = GetSelectedRange(*_owner);
  if (ax_range.IsNull())
    return nil;

  return AXRangeToAXTextMarkerRange(std::move(ax_range));
}

- (NSString*)sortDirection {
  if (![self instanceActive])
    return nil;
  int sortDirection;
  if (!_owner->GetIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                               &sortDirection))
    return nil;

  switch (static_cast<ax::mojom::SortDirection>(sortDirection)) {
    case ax::mojom::SortDirection::kUnsorted:
      return nil;
    case ax::mojom::SortDirection::kAscending:
      return NSAccessibilityAscendingSortDirectionValue;
    case ax::mojom::SortDirection::kDescending:
      return NSAccessibilityDescendingSortDirectionValue;
    case ax::mojom::SortDirection::kOther:
      return NSAccessibilityUnknownSortDirectionValue;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  return nil;
}

// Returns a text marker that points to the first character in the document that
// can be selected with VoiceOver.
- (id)startTextMarker {
  if (![self instanceActive])
    return nil;
  AXPosition position = _owner->CreateTextPositionAt(0);
  return AXPositionToAXTextMarker(position->CreatePositionAtStartOfContent());
}

- (NSString*)AXSubrole {
  return [self subrole];
}

// Returns a subrole based upon the role.
- (NSString*)subrole {
  if (![self instanceActive])
    return nil;

  if (_owner->IsAtomicTextField() &&
      GetState(_owner, ax::mojom::State::kProtected)) {
    return NSAccessibilitySecureTextFieldSubrole;
  }

  if ([self internalRole] == ax::mojom::Role::kDescriptionList)
    return NSAccessibilityDefinitionListSubrole;

  if ([self internalRole] == ax::mojom::Role::kDirectoryDeprecated ||
      [self internalRole] == ax::mojom::Role::kList) {
    return NSAccessibilityContentListSubrole;
  }

  return [AXPlatformNodeCocoa nativeSubroleFromAXRole:[self internalRole]];
}

// Returns all tabs in this subtree.
- (NSArray*)tabs {
  if (![self instanceActive])
    return nil;
  NSMutableArray* tabSubtree = [[NSMutableArray alloc] init];

  if ([self internalRole] == ax::mojom::Role::kTab)
    [tabSubtree addObject:self];

  for (uint i = 0; i < [[self children] count]; ++i) {
    NSArray* tabChildren = [[[self children] objectAtIndex:i] tabs];
    if ([tabChildren count] > 0)
      [tabSubtree addObjectsFromArray:tabChildren];
  }

  return tabSubtree;
}

- (id)AXValue {
  return [self value];
}
- (id)value {
  if (![self instanceActive])
    return nil;

  DCHECK(_owner->node()->IsDataValid());

  if (ui::IsNameExposedInAXValueForRole([self internalRole])) {
    std::u16string name = _owner->GetTextContentUTF16();
    // Leaf node with aria-label will have empty text content.
    // e.g. <div role="option" aria-label="label">content</div>
    // So we use its computed name for AXValue.
    if (name.empty())
      name = _owner->GetNameAsString16();
    if (!IsSelectedStateRelevant(_owner)) {
      return base::SysUTF16ToNSString(name);
    }

    // Append the selection state as a string, because VoiceOver will not
    // automatically report selection state when an individual item is focused.
    bool is_selected =
        _owner->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
    int msg_id =
        is_selected ? IDS_AX_OBJECT_SELECTED : IDS_AX_OBJECT_NOT_SELECTED;
    std::u16string name_with_selection = base::ReplaceStringPlaceholders(
        _owner->GetLocalizedString(msg_id), {name}, nullptr);

    return base::SysUTF16ToNSString(name_with_selection);
  }

  NSString* role = [self role];
  if (ui::IsHeading(_owner->GetRole())) {
    int level = 0;
    if (_owner->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                                &level)) {
      return @(level);
    }
  } else if ([role isEqualToString:NSAccessibilityButtonRole]) {
    // AXValue does not make sense for pure buttons.
    return @"";
  } else if ([self isCheckable]) {
    int value;
    const auto checkedState = _owner->GetData().GetCheckedState();
    switch (checkedState) {
      case ax::mojom::CheckedState::kTrue:
        value = 1;
        break;
      case ax::mojom::CheckedState::kMixed:
        value = 2;
        break;
      default:
        value = _owner->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)
                    ? 1
                    : 0;
        break;
    }
    return @(value);
  } else if (_owner->GetData().IsRangeValueSupported()) {
    // Objects that support range values include progress bars, sliders, and
    // steppers. Only the native value or aria-valuenow should be exposed, not
    // the aria-valuetext. Aria-valuetext is exposed via
    // "accessibilityValueDescription".
    float floatValue;
    if (_owner->GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                                  &floatValue)) {
      return @(floatValue);
    }
  } else if ([role isEqualToString:NSAccessibilityColorWellRole]) {
    unsigned int color = static_cast<unsigned int>(
        _owner->GetIntAttribute(ax::mojom::IntAttribute::kColorValue));
    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    // This string matches the one returned by a native Mac color well.
    return [NSString stringWithFormat:@"rgb %7.5f %7.5f %7.5f 1", red / 255.,
                                      green / 255., blue / 255.];
  }

  return base::SysUTF16ToNSString(_owner->GetValueForControl());
}

- (NSNumber*)valueAutofillAvailable {
  if (![self instanceActive])
    return nil;
  return _owner->HasState(ax::mojom::State::kAutofillAvailable) ? @YES : @NO;
}

// Not currently supported, as Chrome does not store whether an autofill
// occurred. We could have autofill fire an event, however, and set an
// "is_autofilled" flag until the next edit. - (NSNumber*)valueAutofilled {
//  return @NO;
// }

// Not currently supported, as Chrome's autofill types aren't like Safari's.
// - (NSString*)valueAutofillType {
//  return @"none";
//}

- (NSString*)valueDescription {
  if (![self instanceActive] || !_owner->GetData().IsRangeValueSupported())
    return nil;

  // This method is only for exposing aria-valuetext to VoiceOver if present.
  // Blink places the value of aria-valuetext in
  // ax::mojom::StringAttribute::kValue for objects that support range values,
  // i.e., progress bars, sliders and steppers.
  return base::SysUTF8ToNSString(
      _owner->GetStringAttribute(ax::mojom::StringAttribute::kValue));
}

- (NSValue*)AXVisibleCharacterRange {
  return [self visibleCharacterRange];
}

- (NSValue*)visibleCharacterRange {
  if ([self instanceActive] && _owner->IsTextField() &&
      !_owner->IsPasswordField()) {
    return [NSValue
        valueWithRange:NSMakeRange(0,
                                   static_cast<int>(
                                       _owner->GetValueForControl().size()))];
  }
  return nil;
}

- (NSArray*)visibleCells {
  if (![self instanceActive])
    return nil;

  NSMutableArray* ret = [[NSMutableArray alloc] init];
  for (int32_t id : _owner->node()->GetTableUniqueCellIds()) {
    BrowserAccessibility* cell = _owner->manager()->GetFromID(id);
    if (cell)
      [ret addObject:cell->GetNativeViewAccessible()];
  }
  return ret;
}

- (NSArray*)visibleChildren {
  if (![self instanceActive])
    return nil;
  return [self children];
}

- (NSArray*)visibleColumns {
  if (![self instanceActive])
    return nil;
  return [self columns];
}

- (NSArray*)visibleRows {
  if (![self instanceActive])
    return nil;
  return [self accessibilityRows];
}

- (id)window {
  if (![self instanceActive])
    return nil;

  BrowserAccessibilityManagerMac* root_manager =
      _owner->manager()
          ->GetManagerForRootFrame()
          ->ToBrowserAccessibilityManagerMac();
  if (!root_manager) {
    // TODO(crbug.com/40234203) Find out why this happens -- there should always
    // be a root manager whenever an object is instanceActive. This used to be a
    // CHECK() but caused too many crashes, with unknown cause.
    return nil;
  }
  if (!root_manager->GetParentView()) {
    // TODO(crbug.com/40898856) Find out why this happens, there should always
    // be a parent view. This used to be a CHECK() but caused too many crashes.
    // Repro steps are available in the bug.
    return nil;
  }
  return root_manager->GetWindow();  // Can be null for inactive tabs.
}

- (void)getTreeItemDescendantNodeIds:(std::vector<int32_t>*)tree_item_ids {
  for (auto it = _owner->PlatformChildrenBegin();
       it != _owner->PlatformChildrenEnd(); ++it) {
    const BrowserAccessibilityCocoa* child = it->GetNativeViewAccessible();

    if ([child internalRole] == ax::mojom::Role::kTreeItem) {
      tree_item_ids->push_back([child hash]);
    }
    [child getTreeItemDescendantNodeIds:tree_item_ids];
  }
}

- (NSString*)methodNameForAttribute:(NSString*)attribute {
  return [gAttributeToMethodNameMap objectForKey:attribute];
}

- (NSString*)valueForRange:(NSRange)range {
  if (![self instanceActive])
    return nil;

  std::u16string textContent = _owner->GetTextContentUTF16();
  if (NSMaxRange(range) > textContent.length())
    return nil;

  return base::SysUTF16ToNSString(
      textContent.substr(range.location, range.length));
}

- (NSRect)frameForRange:(NSRange)range {
  if (!_owner->IsText() && !_owner->IsAtomicTextField())
    return CGRectNull;
  gfx::Rect rect = _owner->GetUnclippedRootFrameInnerTextRangeBoundsRect(
      range.location, NSMaxRange(range));
  return [self rectInScreen:rect];
}

// Returns the accessibility value for the given attribute.  If the value isn't
// supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityAttributeValue",
               "role=", ui::ToString([self internalRole]),
               "attribute=", base::SysNSStringToUTF8(attribute));
  if (![self instanceActive])
    return nil;

  SEL selector = NSSelectorFromString([self methodNameForAttribute:attribute]);
  if (selector)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    return [self performSelector:selector];
#pragma clang diagnostic pop

  return [super accessibilityAttributeValue:attribute];
}

- (id)AXStringForRange:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSValue class]]);
  return [self valueForRange:[(NSValue*)parameter rangeValue]];
}

- (id)AXLineForIndex:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSNumber class]]);
  int lineIndex = [(NSNumber*)parameter intValue];
  const std::vector<int> lineStarts =
      _owner->GetIntListAttribute(ax::mojom::IntListAttribute::kLineStarts);
  auto iterator =
      std::lower_bound(lineStarts.begin(), lineStarts.end(), lineIndex);
  return @(std::distance(lineStarts.begin(), iterator));
}

- (id)AXRangeForLine:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSNumber class]]);
  if (!_owner->IsTextField())
    return nil;

  int lineIndex = [(NSNumber*)parameter intValue];
  const std::vector<int> lineStarts =
      _owner->GetIntListAttribute(ax::mojom::IntListAttribute::kLineStarts);
  std::u16string value = _owner->GetValueForControl();
  int valueLength = static_cast<int>(value.size());

  int lineCount = static_cast<int>(lineStarts.size());
  if (lineIndex < 0 || lineIndex >= lineCount)
    return nil;
  int start = lineStarts[lineIndex];
  int end =
      (lineIndex < (lineCount - 1)) ? lineStarts[lineIndex + 1] : valueLength;
  return [NSValue valueWithRange:NSMakeRange(start, end - start)];
}

// Returns the accessibility value for the given attribute and parameter. If the
// value isn't supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute
                     forParameter:(id)parameter {
  if (parameter && [parameter isKindOfClass:[NSNumber self]]) {
    TRACE_EVENT2(
        "accessibility",
        "BrowserAccessibilityCocoa::accessibilityAttributeValue:forParameter",
        "role=", ui::ToString([self internalRole]), "attribute=",
        base::SysNSStringToUTF8(attribute) +
            " parameter=" + base::SysNSStringToUTF8([parameter stringValue]));
  } else {
    TRACE_EVENT2(
        "accessibility",
        "BrowserAccessibilityCocoa::accessibilityAttributeValue:forParameter",
        "role=", ui::ToString([self internalRole]),
        "attribute=", base::SysNSStringToUTF8(attribute));
  }

  if (![self instanceActive])
    return nil;

  if ([attribute isEqualToString:
                     NSAccessibilityStringForRangeParameterizedAttribute]) {
    return [self AXStringForRange:parameter];
  }

  if ([attribute
          isEqualToString:NSAccessibilityLineForIndexParameterizedAttribute]) {
    return [self AXLineForIndex:parameter];
  }

  if ([attribute
          isEqualToString:NSAccessibilityRangeForLineParameterizedAttribute]) {
    return [self AXRangeForLine:parameter];
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityCellForColumnAndRowParameterizedAttribute]) {
    if (!ui::IsTableLike([self internalRole]))
      return nil;
    if (![parameter isKindOfClass:[NSArray class]])
      return nil;
    if (2 != [parameter count])
      return nil;
    NSArray* array = parameter;
    int column = [[array objectAtIndex:0] intValue];
    int row = [[array objectAtIndex:1] intValue];

    ui::AXNode* cellNode = _owner->node()->GetTableCellFromCoords(row, column);
    if (!cellNode)
      return nil;

    BrowserAccessibility* cell = _owner->manager()->GetFromID(cellNode->id());
    if (cell)
      return cell->GetNativeViewAccessible();
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityUIElementForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (!position->IsNullPosition()) {
      BrowserAccessibility* ui_element =
          _owner->manager()->GetFromAXNode(position->GetAnchor());
      if (ui_element)
        return ui_element->GetNativeViewAccessible();
    }

    return nil;
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerRangeForUIElementParameterizedAttribute]) {
    if (![parameter isKindOfClass:[AXPlatformNodeCocoa class]])
      return nil;

    BrowserAccessibility* parameter_owner =
        [(BrowserAccessibilityCocoa*)parameter owner];
    if (!parameter_owner)
      return nil;

    AXPosition startPosition = parameter_owner->CreateTextPositionAt(0);
    AXPosition endPosition = startPosition->CreatePositionAtEndOfAnchor();
    AXRange range = AXRange(std::move(startPosition), std::move(endPosition));
    return AXRangeToAXTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityStringForTextMarkerRangeParameterizedAttribute])
    return GetTextForTextMarkerRange(parameter);

  if ([attribute
          isEqualToString:
              NSAccessibilityNextTextMarkerForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;
    return AXPositionToAXTextMarker(
        position->CreateNextCharacterPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kCrossBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition)));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityPreviousTextMarkerForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;
    return AXPositionToAXTextMarker(
        position->CreatePreviousCharacterPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kCrossBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition)));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    AXPosition endPosition = AXTextMarkerToAXPosition(parameter);
    if (endPosition->IsNullPosition())
      return nil;

    AXPosition startWordPosition =
        endPosition->CreatePreviousWordStartPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kStopAtAnchorBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition));
    AXPosition endWordPosition =
        endPosition->CreatePreviousWordEndPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kStopAtAnchorBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition));
    AXPosition startPosition = *startWordPosition <= *endWordPosition
                                   ? std::move(endWordPosition)
                                   : std::move(startWordPosition);
    AXRange range(std::move(startPosition), std::move(endPosition));
    return AXRangeToAXTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityRightWordTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    AXPosition startPosition = AXTextMarkerToAXPosition(parameter);
    if (startPosition->IsNullPosition())
      return nil;

    AXPosition endWordPosition =
        startPosition->CreateNextWordEndPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kStopAtAnchorBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition));
    AXPosition startWordPosition =
        startPosition->CreateNextWordStartPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kStopAtAnchorBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition));
    AXPosition endPosition = *startWordPosition <= *endWordPosition
                                 ? std::move(startWordPosition)
                                 : std::move(endWordPosition);
    AXRange range(std::move(startPosition), std::move(endPosition));
    return AXRangeToAXTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityNextWordEndTextMarkerForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;
    return AXPositionToAXTextMarker(
        position->CreateNextWordEndPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kCrossBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition)));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;
    return AXPositionToAXTextMarker(
        position->CreatePreviousWordStartPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kCrossBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition)));
  }

  if ([attribute isEqualToString:
                     NSAccessibilityLineForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;

    int textOffset = position->AsTextPosition()->text_offset();
    const std::vector<int> lineStarts =
        _owner->GetIntListAttribute(ax::mojom::IntListAttribute::kLineStarts);
    const auto iterator =
        std::lower_bound(lineStarts.begin(), lineStarts.end(), textOffset);
    return @(std::distance(lineStarts.begin(), iterator));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerRangeForLineParameterizedAttribute]) {
    int lineIndex = [(NSNumber*)parameter intValue];
    const std::vector<int> lineStarts =
        _owner->GetIntListAttribute(ax::mojom::IntListAttribute::kLineStarts);
    int lineCount = static_cast<int>(lineStarts.size());
    if (lineIndex < 0 || lineIndex >= lineCount)
      return nil;

    int lineStartOffset = lineStarts[lineIndex];
    AXPosition lineStartPosition = _owner->CreateTextPositionAt(
        lineStartOffset, ax::mojom::TextAffinity::kDownstream);
    if (lineStartPosition->IsNullPosition())
      return nil;

    // Make sure that the line start position is really at the start of the
    // current line.
    lineStartPosition = lineStartPosition->CreatePreviousLineStartPosition(
        ui::AXMovementOptions(ui::AXBoundaryBehavior::kStopAtAnchorBoundary,
                              ui::AXBoundaryDetection::kCheckInitialPosition));
    AXPosition lineEndPosition =
        lineStartPosition->CreateNextLineEndPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kStopAtAnchorBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition));
    AXRange range(std::move(lineStartPosition), std::move(lineEndPosition));
    return AXRangeToAXTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    AXPosition endPosition = AXTextMarkerToAXPosition(parameter);
    if (endPosition->IsNullPosition())
      return nil;

    AXPosition startLinePosition =
        endPosition->CreatePreviousLineStartPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kStopAtLastAnchorBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition));
    AXPosition endLinePosition =
        endPosition->CreatePreviousLineEndPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kStopAtLastAnchorBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition));
    AXPosition startPosition = *startLinePosition <= *endLinePosition
                                   ? std::move(endLinePosition)
                                   : std::move(startLinePosition);
    AXRange range(std::move(startPosition), std::move(endPosition));
    return AXRangeToAXTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityRightLineTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    AXPosition startPosition = AXTextMarkerToAXPosition(parameter);
    if (startPosition->IsNullPosition())
      return nil;

    AXPosition startLinePosition =
        startPosition->CreateNextLineStartPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kStopAtLastAnchorBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition));
    AXPosition endLinePosition =
        startPosition->CreateNextLineEndPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kStopAtLastAnchorBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition));
    AXPosition endPosition = *startLinePosition <= *endLinePosition
                                 ? std::move(startLinePosition)
                                 : std::move(endLinePosition);
    AXRange range(std::move(startPosition), std::move(endPosition));
    return AXRangeToAXTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityNextLineEndTextMarkerForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;
    return AXPositionToAXTextMarker(
        position->CreateNextLineEndPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kCrossBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition)));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;
    return AXPositionToAXTextMarker(
        position->CreatePreviousLineStartPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kCrossBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition)));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilitySentenceTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;

    AXRange range = position->ExpandToEnclosingTextBoundary(
        ax::mojom::TextBoundary::kSentenceStartOrEnd,
        ui::AXRangeExpandBehavior::kLeftFirst);
    return AXRangeToAXTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityParagraphTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;

    AXRange range = position->ExpandToEnclosingTextBoundary(
        ax::mojom::TextBoundary::kParagraphStartOrEnd,
        ui::AXRangeExpandBehavior::kLeftFirst);
    return AXRangeToAXTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;
    return AXPositionToAXTextMarker(
        position->CreateNextParagraphEndPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kCrossBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition)));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;
    return AXPositionToAXTextMarker(
        position->CreatePreviousParagraphStartPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kCrossBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition)));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityStyleTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;

    AXPosition startPosition = position->CreatePreviousFormatStartPosition(
        ui::AXMovementOptions(ui::AXBoundaryBehavior::kStopAtAnchorBoundary,
                              ui::AXBoundaryDetection::kCheckInitialPosition));
    AXPosition endPosition = position->CreateNextFormatEndPosition(
        ui::AXMovementOptions(ui::AXBoundaryBehavior::kStopAtAnchorBoundary,
                              ui::AXBoundaryDetection::kCheckInitialPosition));
    AXRange range(std::move(startPosition), std::move(endPosition));
    return AXRangeToAXTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityLengthForTextMarkerRangeParameterizedAttribute]) {
    NSString* text = GetTextForTextMarkerRange(parameter);
    return @([text length]);
  }

  if ([attribute isEqualToString:
                     NSAccessibilityTextMarkerIsValidParameterizedAttribute]) {
    return @(AXTextMarkerToAXPosition(parameter)->IsNullPosition());
  }

  if ([attribute isEqualToString:
                     NSAccessibilityIndexForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;
    return @(position->AsTextPosition()->text_offset());
  }

  if ([attribute isEqualToString:
                     NSAccessibilityTextMarkerForIndexParameterizedAttribute]) {
    int index = [static_cast<NSNumber*>(parameter) intValue];
    if (index < 0)
      return nil;

    const BrowserAccessibility* root =
        _owner->manager()->GetBrowserAccessibilityRoot();
    if (!root)
      return nil;

    return AXPositionToAXTextMarker(root->CreateTextPositionAt(index));
  }

  if ([attribute isEqualToString:
                     NSAccessibilityBoundsForRangeParameterizedAttribute]) {
    NSRect rect = [self frameForRange:[(NSValue*)parameter rangeValue]];
    return CGRectIsNull(rect) ? nil : [NSValue valueWithRect:rect];
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute]) {
    OneShotAccessibilityTreeSearch search(_owner);
    if (InitializeAccessibilityTreeSearch(&search, parameter))
      return @(search.CountMatches());
    return nil;
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute]) {
    OneShotAccessibilityTreeSearch search(_owner);
    if (InitializeAccessibilityTreeSearch(&search, parameter)) {
      size_t count = search.CountMatches();
      NSMutableArray* result = [NSMutableArray arrayWithCapacity:count];
      for (size_t i = 0; i < count; ++i) {
        BrowserAccessibility* match = search.GetMatchAtIndex(i);
        [result addObject:match->GetNativeViewAccessible()];
      }
      return result;
    }
    return nil;
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return nil;

    // If the initial position is between lines, e.g. if it is on a soft line
    // break or on an ignored position that separates lines, we have to return
    // the previous line. This is what Safari does.
    //
    // Note that hard line breaks are on a line of their own.
    AXPosition startPosition = position->CreatePreviousLineStartPosition(
        ui::AXMovementOptions(ui::AXBoundaryBehavior::kStopAtAnchorBoundary,
                              ui::AXBoundaryDetection::kCheckInitialPosition));
    AXPosition endPosition =
        startPosition->CreateNextLineStartPosition(ui::AXMovementOptions(
            ui::AXBoundaryBehavior::kStopAtLastAnchorBoundary,
            ui::AXBoundaryDetection::kDontCheckInitialPosition));
    AXRange range(std::move(startPosition), std::move(endPosition));
    return AXRangeToAXTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityBoundsForTextMarkerRangeParameterizedAttribute]) {
    BrowserAccessibility* startObject;
    BrowserAccessibility* endObject;
    int startOffset, endOffset;
    AXRange range = AXTextMarkerRangeToAXRange(parameter);
    if (range.IsNull())
      return nil;

    const AXPosition anchor = range.anchor()->AsTextPosition();
    const AXPosition focus = range.focus()->AsTextPosition();
    if (!anchor || !focus) {
      return nil;
    }

    startObject = _owner->manager()->GetFromAXNode(anchor->GetAnchor());
    endObject = _owner->manager()->GetFromAXNode(focus->GetAnchor());
    startOffset = anchor->text_offset();
    endOffset = focus->text_offset();
    DCHECK(startObject && endObject);
    DCHECK_GE(startOffset, 0);
    DCHECK_GE(endOffset, 0);
    if (!startObject || !endObject || startOffset < 0 || endOffset < 0)
      return nil;

    gfx::Rect rect =
        BrowserAccessibilityManager::GetRootFrameInnerTextRangeBoundsRect(
            *startObject, startOffset, *endObject, endOffset);
    NSRect nsrect = [self rectInScreen:rect];
    return [NSValue valueWithRect:nsrect];
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute]) {
    if (![parameter isKindOfClass:[NSArray class]])
      return nil;

    NSArray* textMarkerArray = parameter;
    if ([textMarkerArray count] != 2)
      return nil;

    AXPosition startPosition =
        AXTextMarkerToAXPosition([textMarkerArray objectAtIndex:0]);
    AXPosition endPosition =
        AXTextMarkerToAXPosition([textMarkerArray objectAtIndex:1]);
    if (*startPosition <= *endPosition) {
      return AXRangeToAXTextMarkerRange(
          AXRange(std::move(startPosition), std::move(endPosition)));
    } else {
      return AXRangeToAXTextMarkerRange(
          AXRange(std::move(endPosition), std::move(startPosition)));
    }
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerDebugDescriptionParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    return base::SysUTF8ToNSString(position->ToString());
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerRangeDebugDescriptionParameterizedAttribute]) {
    AXRange range = AXTextMarkerRangeToAXRange(parameter);
    return base::SysUTF8ToNSString(range.ToString());
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerNodeDebugDescriptionParameterizedAttribute]) {
    AXPosition position = AXTextMarkerToAXPosition(parameter);
    if (position->IsNullPosition())
      return @"nil";
    DCHECK(position->GetAnchor());
    return base::SysUTF8ToNSString(position->GetAnchor()->data().ToString());
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityIndexForChildUIElementParameterizedAttribute]) {
    if (![parameter isKindOfClass:[AXPlatformNodeCocoa class]])
      return nil;

    BrowserAccessibilityCocoa* childCocoaObj =
        (BrowserAccessibilityCocoa*)parameter;
    BrowserAccessibility* child = [childCocoaObj owner];
    if (!child)
      return nil;

    if (child->PlatformGetParent() != _owner)
      return nil;

    return @(child->GetIndexInParent().value());
  }

  return [super accessibilityAttributeValue:attribute forParameter:parameter];
}

// Returns an array of parameterized attributes names that this object will
// respond to.
- (NSArray*)accessibilityParameterizedAttributeNames {
  TRACE_EVENT1(
      "accessibility",
      "BrowserAccessibilityCocoa::accessibilityParameterizedAttributeNames",
      "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return nil;

  // General attributes.
  NSMutableArray* ret = [@[
    NSAccessibilityUIElementForTextMarkerParameterizedAttribute,
    NSAccessibilityTextMarkerRangeForUIElementParameterizedAttribute,
    NSAccessibilityLineForTextMarkerParameterizedAttribute,
    NSAccessibilityTextMarkerRangeForLineParameterizedAttribute,
    NSAccessibilityStringForTextMarkerRangeParameterizedAttribute,
    NSAccessibilityTextMarkerForPositionParameterizedAttribute,
    NSAccessibilityBoundsForTextMarkerRangeParameterizedAttribute,
    NSAccessibilityAttributedStringForTextMarkerRangeWithOptionsParameterizedAttribute,
    NSAccessibilityTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute,
    NSAccessibilityNextTextMarkerForTextMarkerParameterizedAttribute,
    NSAccessibilityPreviousTextMarkerForTextMarkerParameterizedAttribute,
    NSAccessibilityLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute,
    NSAccessibilityRightWordTextMarkerRangeForTextMarkerParameterizedAttribute,
    NSAccessibilityLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute,
    NSAccessibilityRightLineTextMarkerRangeForTextMarkerParameterizedAttribute,
    NSAccessibilitySentenceTextMarkerRangeForTextMarkerParameterizedAttribute,
    NSAccessibilityParagraphTextMarkerRangeForTextMarkerParameterizedAttribute,
    NSAccessibilityNextWordEndTextMarkerForTextMarkerParameterizedAttribute,
    NSAccessibilityPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute,
    NSAccessibilityNextLineEndTextMarkerForTextMarkerParameterizedAttribute,
    NSAccessibilityPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute,
    NSAccessibilityNextSentenceEndTextMarkerForTextMarkerParameterizedAttribute,
    NSAccessibilityPreviousSentenceStartTextMarkerForTextMarkerParameterizedAttribute,
    NSAccessibilityNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute,
    NSAccessibilityPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute,
    NSAccessibilityStyleTextMarkerRangeForTextMarkerParameterizedAttribute,
    NSAccessibilityLengthForTextMarkerRangeParameterizedAttribute,
    NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute,
    NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute,
    NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute,
    NSAccessibilityIndexForChildUIElementParameterizedAttribute,
    NSAccessibilityBoundsForRangeParameterizedAttribute,
    NSAccessibilityStringForRangeParameterizedAttribute,
    NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute,
    NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute,
    NSAccessibilitySelectTextWithCriteriaParameterizedAttribute
  ] mutableCopy];

  if ([[self role] isEqualToString:NSAccessibilityTableRole] ||
      [[self role] isEqualToString:NSAccessibilityGridRole]) {
    [ret addObject:NSAccessibilityCellForColumnAndRowParameterizedAttribute];
  }

  if (_owner->HasState(ax::mojom::State::kEditable)) {
    [ret addObjectsFromArray:@[
      NSAccessibilityLineForIndexParameterizedAttribute,
      NSAccessibilityRangeForLineParameterizedAttribute,
      NSAccessibilityStringForRangeParameterizedAttribute,
      NSAccessibilityRangeForPositionParameterizedAttribute,
      NSAccessibilityRangeForIndexParameterizedAttribute,
      NSAccessibilityBoundsForRangeParameterizedAttribute,
      NSAccessibilityRTFForRangeParameterizedAttribute,
      NSAccessibilityStyleRangeForIndexParameterizedAttribute
    ]];
  }

  if ([self internalRole] == ax::mojom::Role::kStaticText)
    [ret addObject:NSAccessibilityBoundsForRangeParameterizedAttribute];

  if (ui::IsPlatformDocument(_owner->GetRole())) {
    [ret addObjectsFromArray:@[
      NSAccessibilityTextMarkerIsValidParameterizedAttribute,
      NSAccessibilityIndexForTextMarkerParameterizedAttribute,
      NSAccessibilityTextMarkerForIndexParameterizedAttribute
    ]];
  }

  NSArray* super_ret = [super accessibilityParameterizedAttributeNames];
  [ret addObjectsFromArray:super_ret];
  return ret;
}

// Returns an array of action names that this object will respond to.
- (NSArray*)accessibilityActionNames {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::accessibilityActionNames",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return nil;

  NSMutableArray* actions = [NSMutableArray
      arrayWithObjects:NSAccessibilityShowMenuAction,
                       NSAccessibilityScrollToVisibleAction, nil];

  // VoiceOver expects the "press" action to be first.
  if (_owner->IsClickable())
    [actions insertObject:NSAccessibilityPressAction atIndex:0];

  if (ui::IsMenuRelated(_owner->GetRole()))
    [actions addObject:NSAccessibilityCancelAction];

  if ([self internalRole] == ax::mojom::Role::kSlider ||
      [self internalRole] == ax::mojom::Role::kSpinButton) {
    [actions addObjectsFromArray:@[
      NSAccessibilityIncrementAction, NSAccessibilityDecrementAction
    ]];
  }

  return actions;
}

// Returns the list of accessibility attributes that this object supports.
- (NSMutableArray*)internalAccessibilityAttributeNames {
  if (![self instanceActive])
    return nil;

  // General attributes.
  NSMutableArray* ret = [@[
    NSAccessibilityChildrenAttribute, NSAccessibilityEnabledAttribute,
    NSAccessibilityEndTextMarkerAttribute, NSAccessibilityFocusedAttribute,
    NSAccessibilityLinkedUIElementsAttribute, NSAccessibilityParentAttribute,
    NSAccessibilityPositionAttribute, NSAccessibilityRoleAttribute,
    NSAccessibilityRoleDescriptionAttribute,
    NSAccessibilitySelectedTextMarkerRangeAttribute,
    NSAccessibilityStartTextMarkerAttribute, NSAccessibilitySubroleAttribute,
    NSAccessibilityTopLevelUIElementAttribute, NSAccessibilityValueAttribute,
    NSAccessibilityWindowAttribute
  ] mutableCopy];

  // Specific role attributes.
  NSString* role = [self role];
  NSString* subrole = [self subrole];
  if ([role isEqualToString:NSAccessibilityTableRole] ||
      [role isEqualToString:NSAccessibilityGridRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilityColumnsAttribute,
      NSAccessibilityVisibleColumnsAttribute,
      NSAccessibilityRowsAttribute,
      NSAccessibilityVisibleRowsAttribute,
      NSAccessibilityVisibleCellsAttribute,
      NSAccessibilityHeaderAttribute,
      NSAccessibilityRowHeaderUIElementsAttribute,
    ]];
  } else if ([role isEqualToString:NSAccessibilityColumnRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilityIndexAttribute, NSAccessibilityHeaderAttribute,
      NSAccessibilityRowsAttribute, NSAccessibilityVisibleRowsAttribute
    ]];
  } else if ([role isEqualToString:NSAccessibilityCellRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilityColumnIndexRangeAttribute,
      NSAccessibilityRowIndexRangeAttribute,
      @"AXSortDirection",
    ]];
    if ([self internalRole] != ax::mojom::Role::kRowHeader)
      [ret addObject:NSAccessibilityRowHeaderUIElementsAttribute];
  } else if ([role isEqualToString:NSAccessibilityTabGroupRole]) {
    [ret addObject:NSAccessibilityTabsAttribute];
  } else if (_owner->GetData().IsRangeValueSupported()) {
    [ret addObjectsFromArray:@[
      NSAccessibilityMaxValueAttribute, NSAccessibilityMinValueAttribute,
      NSAccessibilityValueDescriptionAttribute
    ]];
  } else if ([role isEqualToString:NSAccessibilityRowRole]) {
    BrowserAccessibility* container = _owner->PlatformGetParent();
    if (container && container->GetRole() == ax::mojom::Role::kRowGroup)
      container = container->PlatformGetParent();
    if ([subrole isEqualToString:NSAccessibilityOutlineRowSubrole] ||
        (container && container->GetRole() == ax::mojom::Role::kTreeGrid)) {
      // clang-format off
      [ret addObjectsFromArray:@[
        NSAccessibilityIndexAttribute,
        NSAccessibilityDisclosedByRowAttribute,
        NSAccessibilityDisclosedRowsAttribute,
        NSAccessibilityDisclosingAttribute,
        NSAccessibilityDisclosureLevelAttribute
      ]];
      // clang-format on
    } else {
      [ret addObject:NSAccessibilityIndexAttribute];
    }
  } else if ([self internalRole] == ax::mojom::Role::kHeading) {
    // Heading level is exposed in both AXDisclosureLevel and AXValue.
    // Safari also exposes the level in both.
    [ret addObject:NSAccessibilityDisclosureLevelAttribute];
  } else if ([role isEqualToString:NSAccessibilityListRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilitySelectedChildrenAttribute,
      NSAccessibilityVisibleChildrenAttribute
    ]];
  } else if ([role isEqualToString:NSAccessibilityOutlineRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilityRowsAttribute, NSAccessibilityColumnsAttribute,
      NSAccessibilityOrientationAttribute
    ]];
  }

  if (_owner->IsTextField()) {
    [ret addObjectsFromArray:@[
      NSAccessibilityInsertionPointLineNumberAttribute,
      NSAccessibilityNumberOfCharactersAttribute,
      NSAccessibilityPlaceholderValueAttribute,
      NSAccessibilitySelectedTextAttribute,
      NSAccessibilitySelectedTextRangeAttribute,
      NSAccessibilityVisibleCharacterRangeAttribute,
      NSAccessibilityValueAutofillAvailableAttribute,
      // Not currently supported by Chrome:
      // NSAccessibilityValueAutofilledAttribute,
      // Not currently supported by Chrome:
      // NSAccessibilityValueAutofillTypeAttribute
    ]];
  }

  if (GetState(_owner, ax::mojom::State::kExpanded) ||
      GetState(_owner, ax::mojom::State::kCollapsed)) {
    [ret addObject:NSAccessibilityExpandedAttribute];
  }

  if (GetState(_owner, ax::mojom::State::kVertical) ||
      GetState(_owner, ax::mojom::State::kHorizontal)) {
    [ret addObject:NSAccessibilityOrientationAttribute];
  }

  // TODO(accessibility) What nodes should language be exposed on given new
  // auto detection features?
  //
  // Once lang attribute inheritance becomes stable most nodes will have a
  // language, so it may make more sense to always expose this attribute.
  //
  // For now we expose the language attribute if we have any language set.
  if (_owner->node() && !_owner->node()->GetLanguage().empty())
    [ret addObject:NSAccessibilityLanguageAttribute];

  // TODO(aboxhall): expose NSAccessibilityServesAsTitleForUIElementsAttribute
  // for elements which are referred to by labelledby or are labels

  [ret addObjectsFromArray:[super internalAccessibilityAttributeNames]];
  return ret;
}

// Returns the index of the child in this objects array of children.
- (NSUInteger)accessibilityGetIndexOf:(id)child {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::accessibilityGetIndexOf",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return 0;

  NSUInteger index = 0;
  for (AXPlatformNodeCocoa* childToCheck in [self accessibilityChildren]) {
    if ([child isEqual:childToCheck])
      return index;
    ++index;
  }
  return NSNotFound;
}

// Returns whether or not the specified attribute can be set by the
// accessibility API via |accessibilitySetValue:forAttribute:|.
// This API is deprecated.
- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityIsAttributeSettable",
               "role=", ui::ToString([self internalRole]),
               "attribute=", base::SysNSStringToUTF8(attribute));
  if (![self instanceActive])
    return NO;

  if ([self isMigratingAttribute:attribute]) {
    return NO;
  }

  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    if ([self internalRole] == ax::mojom::Role::kDateTime)
      return NO;

    return _owner->IsFocusable();
  }

  if ([attribute isEqualToString:NSAccessibilityValueAttribute])
    return _owner->HasAction(ax::mojom::Action::kSetValue);

  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute] &&
      _owner->HasState(ax::mojom::State::kEditable)) {
    return YES;
  }

  if ([attribute
          isEqualToString:NSAccessibilitySelectedTextMarkerRangeAttribute])
    return YES;

  return NO;
}

- (BOOL)isAccessibilityEnabled {
  if (![self instanceActive])
    return NO;

  return _owner->GetData().GetRestriction() !=
         ax::mojom::Restriction::kDisabled;
}

- (NSRect)accessibilityFrame {
  if (![self instanceActive])
    return NSZeroRect;

  BrowserAccessibilityManager* manager = _owner->manager();

  // Clipping a table's cells results in cells with zero height or width. This
  // causes VoiceOver to treat these cells as if they don't exist at all,
  // making it impossible for the user to use VO to navigate to them. Instead,
  // make sure all rows and cells have an extent, even if not visible.
  ax::mojom::Role role = _owner->GetRole();
  bool isTableComponent = ui::IsTableColumn(role) || ui::IsTableRow(role) ||
                          ui::IsCellOrTableHeader(role);
  ui::AXClippingBehavior clipping_behavior =
      isTableComponent ? ui::AXClippingBehavior::kUnclipped
                       : ui::AXClippingBehavior::kClipped;
  auto rect = _owner->GetBoundsRect(ui::AXCoordinateSystem::kScreenDIPs,
                                    clipping_behavior);

  // TODO(vmpstr): GetBoundsRect() call above should account for this instead.
  auto result_rect =
      ScaleToRoundedRect(rect, 1.f / manager->device_scale_factor());

  return gfx::ScreenRectToNSRect(result_rect);
}

- (BOOL)isCheckable {
  if (![self instanceActive])
    return NO;

  return _owner->GetData().HasCheckedState() ||
         _owner->GetRole() == ax::mojom::Role::kTab;
}

// Performs the given accessibility action on the webkit accessibility object
// that backs this object.
- (void)accessibilityPerformAction:(NSString*)action {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityPerformAction",
               "role=", ui::ToString([self internalRole]),
               "action=", base::SysNSStringToUTF8(action));
  if (![self instanceActive])
    return;

  // TODO(dmazzoni): Support more actions.
  BrowserAccessibility* actionTarget = [self actionTarget];
  BrowserAccessibilityManager* manager = actionTarget->manager();
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    manager->DoDefaultAction(*actionTarget);
    if (actionTarget->GetData().GetRestriction() !=
            ax::mojom::Restriction::kNone ||
        ![self isCheckable])
      return;
    // Hack: preemptively set the checked state to what it should become,
    // otherwise VoiceOver will very likely report the old, incorrect state to
    // the user as it requests the value too quickly.
    ui::AXNode* node = actionTarget->node();
    if (!node)
      return;
    AXNodeData data(node->TakeData());  // Temporarily take data.
    if (data.role == ax::mojom::Role::kRadioButton) {
      data.SetCheckedState(ax::mojom::CheckedState::kTrue);
    } else if (data.role == ax::mojom::Role::kCheckBox ||
               data.role == ax::mojom::Role::kSwitch ||
               data.role == ax::mojom::Role::kToggleButton) {
      ax::mojom::CheckedState checkedState = data.GetCheckedState();
      ax::mojom::CheckedState newCheckedState =
          checkedState == ax::mojom::CheckedState::kFalse
              ? ax::mojom::CheckedState::kTrue
              : ax::mojom::CheckedState::kFalse;
      data.SetCheckedState(newCheckedState);
    }
    node->SetData(data);  // Set the data back in the node.
  } else if ([action isEqualToString:NSAccessibilityShowMenuAction]) {
    manager->ShowContextMenu(*actionTarget);
  } else if ([action isEqualToString:NSAccessibilityScrollToVisibleAction]) {
    manager->ScrollToMakeVisible(*actionTarget, gfx::Rect());
  } else if ([action isEqualToString:NSAccessibilityIncrementAction]) {
    manager->Increment(*actionTarget);
  } else if ([action isEqualToString:NSAccessibilityDecrementAction]) {
    manager->Decrement(*actionTarget);
  }
}

// Returns the description of the given action.
- (NSString*)accessibilityActionDescription:(NSString*)action {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityActionDescription",
               "role=", ui::ToString([self internalRole]),
               "action=", base::SysNSStringToUTF8(action));
  if (![self instanceActive])
    return nil;

  return NSAccessibilityActionDescription(action);
}

// Sets an override value for a specific accessibility attribute.
// This class does not support this.
- (BOOL)accessibilitySetOverrideValue:(id)value
                         forAttribute:(NSString*)attribute {
  TRACE_EVENT2(
      "accessibility",
      "BrowserAccessibilityCocoa::accessibilitySetOverrideValue:forAttribute",
      "role=", ui::ToString([self internalRole]),
      "attribute=", base::SysNSStringToUTF8(attribute));
  if (![self instanceActive])
    return NO;
  return NO;
}

// Sets the value for an accessibility attribute via the accessibility API.
// This API is deprecated.
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilitySetValue:forAttribute",
               "role=", ui::ToString([self internalRole]),
               "attribute=", base::SysNSStringToUTF8(attribute));
  if (![self instanceActive])
    return;

  if ([self isMigratingAttribute:attribute]) {
    return;
  }

  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    [self setAccessibilityFocused:[value boolValue]];
  }
  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
    if (ui::IsNSRange(value)) {
      [self setAccessibilitySelectedTextRange:[(NSValue*)value rangeValue]];
    }
  }
  if ([attribute
          isEqualToString:NSAccessibilitySelectedTextMarkerRangeAttribute] &&
      // Condition also on when this node is editable. VoiceOver as of Mac 13
      // sets selections as users navigate on read only content. This has
      // adverse side effects on VoiceOver's a11y focus causing loops in
      // navigation.
      _owner->HasState(ax::mojom::State::kEditable)) {
    AXRange range = AXTextMarkerRangeToAXRange(value);
    if (range.IsNull())
      return;
    BrowserAccessibilityManager* manager = _owner->manager();
    manager->SetSelection(AXRange(range.anchor()->AsDomSelectionPosition(),
                                  range.focus()->AsDomSelectionPosition()));
  }
}

- (id)accessibilityFocusedUIElement {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::accessibilityFocusedUIElement",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return nil;

  ui::AXPlatformNodeDelegate* focus_node = _owner->manager()->GetFocus();
  return focus_node ? focus_node->GetNativeViewAccessible() : nullptr;
}

// Returns the deepest accessibility child that should not be ignored.
// It is assumed that the hit test has been narrowed down to this object
// or one of its children, so this will never return nil unless this
// object is invalid.
- (id)accessibilityHitTest:(NSPoint)point {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityHitTest",
               "role=", ui::ToString([self internalRole]),
               "point=", base::SysNSStringToUTF8(NSStringFromPoint(point)));
  if (![self instanceActive])
    return nil;

  // The point we receive is in frame coordinates.
  // Convert to screen coordinates and then to physical pixel coordinates.
  BrowserAccessibilityManager* manager = _owner->manager();
  gfx::Point screen_point_in_dips(point.x, point.y);

  auto offset_in_blink_space =
      manager->GetViewBoundsInScreenCoordinates().OffsetFromOrigin();

  // Blink space is physical, so we scale the
  // point first then add the offset. Otherwise, it's in DIPs so we add the
  // offset first and then scale.
  // TODO(vmpstr): GetViewBoundsInScreenCoordinates should return consistent
  // space.
  gfx::Point screen_point_in_physical_space;
  screen_point_in_physical_space =
      ScaleToRoundedPoint(screen_point_in_dips, manager->device_scale_factor());
  screen_point_in_physical_space += offset_in_blink_space;

  BrowserAccessibility* hit =
      manager->CachingAsyncHitTest(screen_point_in_physical_space);
  if (!hit)
    return nil;

  return NSAccessibilityUnignoredAncestor(hit->GetNativeViewAccessible());
}

- (BOOL)isEqual:(id)object {
  if (![object isKindOfClass:[BrowserAccessibilityCocoa class]])
    return NO;
  return ([self hash] == [object hash]);
}

- (NSUInteger)hash {
  // Potentially called during dealloc.
  if (![self instanceActive])
    return [super hash];
  return _owner->GetId();
}

- (BOOL)accessibilityNotifiesWhenDestroyed {
  TRACE_EVENT0("accessibility",
               "BrowserAccessibilityCocoa::accessibilityNotifiesWhenDestroyed");
  // Indicate that BrowserAccessibilityCocoa will post a notification when it's
  // destroyed (see -detach). This allows VoiceOver to do some internal things
  // more efficiently.
  return YES;
}

// Choose the appropriate accessibility object to receive an action depending
// on the characteristics of this accessibility node.
- (BrowserAccessibility*)actionTarget {
  // When an action is triggered on a container with selectable children and
  // one of those children is an active descendant or focused, retarget the
  // action to that child. See https://crbug.com/1114892.
  if (!ui::IsContainerWithSelectableChildren(_owner->GetRole()))
    return _owner;

  // Active descendant takes priority over focus, because the webpage author has
  // explicitly designated a different behavior for users of assistive software.
  BrowserAccessibility* activeDescendant =
      _owner->manager()->GetActiveDescendant(_owner);
  if (activeDescendant != _owner)
    return activeDescendant;

  BrowserAccessibility* focus = _owner->manager()->GetFocus();
  if (focus && focus->IsDescendantOf(_owner))
    return focus;

  return _owner;
}

- (BOOL)isAccessibilityElement {
  if (![self instanceActive])
    return NO;

  if ([self internalRole] == ax::mojom::Role::kImage &&
      _owner->HasExplicitlyEmptyName()) {
    return NO;
  }

  return [super isAccessibilityElement];
}

- (id)accessibilityDisclosedByRow {
  if (![self instanceActive]) {
    return nil;
  }

  // The row that contains this row.
  // It should be the same as the first parent that is a treeitem.
  return nil;
}

- (id)accessibilityDisclosedRows {
  if (![self instanceActive]) {
    return nil;
  }

  // The rows that are considered inside this row.
  return nil;
}

- (NSInteger)accessibilityDisclosureLevel {
  if (![self instanceActive]) {
    return 0;
  }

  ax::mojom::Role role = [self internalRole];
  if (role == ax::mojom::Role::kRow || role == ax::mojom::Role::kTreeItem ||
      role == ax::mojom::Role::kHeading) {
    int level =
        _owner->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
    // Mac disclosureLevel is 0-based, but web levels are 1-based.
    if (level > 0) {
      level--;
    }
    return level;
  }

  return 0;
}

- (BOOL)isAccessibilityDisclosed {
  if (![self instanceActive]) {
    return NO;
  }

  if ([self internalRole] == ax::mojom::Role::kTreeItem) {
    return GetState(_owner, ax::mojom::State::kExpanded);
  }

  return NO;
}

@end
