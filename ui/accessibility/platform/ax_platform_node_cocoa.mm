// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/accessibility/platform/ax_platform_node_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/platform/ax_platform_node_mac.h"
#include "ui/accessibility/platform/ax_private_attributes_mac.h"
#include "ui/accessibility/platform/ax_private_roles_mac.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/strings/grit/ax_strings.h"

namespace ui {

AXAnnouncementSpec::AXAnnouncementSpec() = default;
AXAnnouncementSpec::~AXAnnouncementSpec() = default;

}  // namespace ui

namespace {

// Same length as web content/WebKit.
static int kLiveRegionDebounceMillis = 20;

using RoleMap = std::map<ax::mojom::Role, NSString*>;
using EventMap = std::map<ax::mojom::Event, NSString*>;
using ActionList = std::vector<std::pair<ax::mojom::Action, NSString*>>;

RoleMap BuildRoleMap() {
  // TODO(accessibility) Are any missing? Consider switch statement so that
  // compiler doesn't allow missing roles;
  const RoleMap::value_type roles[] = {
      {ax::mojom::Role::kAbbr, NSAccessibilityGroupRole},
      {ax::mojom::Role::kAlert, NSAccessibilityGroupRole},
      {ax::mojom::Role::kAlertDialog, NSAccessibilityGroupRole},
      {ax::mojom::Role::kApplication, NSAccessibilityGroupRole},
      {ax::mojom::Role::kArticle, NSAccessibilityGroupRole},
      {ax::mojom::Role::kAudio, NSAccessibilityGroupRole},
      {ax::mojom::Role::kBanner, NSAccessibilityGroupRole},
      {ax::mojom::Role::kBlockquote, NSAccessibilityGroupRole},
      {ax::mojom::Role::kButton, NSAccessibilityButtonRole},
      {ax::mojom::Role::kCanvas, NSAccessibilityImageRole},
      {ax::mojom::Role::kCaption, NSAccessibilityGroupRole},
      {ax::mojom::Role::kCell, @"AXCell"},
      {ax::mojom::Role::kCheckBox, NSAccessibilityCheckBoxRole},
      {ax::mojom::Role::kCode, NSAccessibilityGroupRole},
      {ax::mojom::Role::kColorWell, NSAccessibilityColorWellRole},
      {ax::mojom::Role::kColumn, NSAccessibilityColumnRole},
      {ax::mojom::Role::kColumnHeader, @"AXCell"},
      {ax::mojom::Role::kComboBoxGrouping, NSAccessibilityComboBoxRole},
      {ax::mojom::Role::kComboBoxMenuButton, NSAccessibilityComboBoxRole},
      {ax::mojom::Role::kComment, NSAccessibilityGroupRole},
      {ax::mojom::Role::kComplementary, NSAccessibilityGroupRole},
      {ax::mojom::Role::kContentDeletion, NSAccessibilityGroupRole},
      {ax::mojom::Role::kContentInsertion, NSAccessibilityGroupRole},
      {ax::mojom::Role::kContentInfo, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDate, @"AXDateField"},
      {ax::mojom::Role::kDateTime, @"AXDateField"},
      {ax::mojom::Role::kDefinition, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDescriptionListDetail, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDescriptionList, NSAccessibilityListRole},
      {ax::mojom::Role::kDescriptionListTerm, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDialog, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDetails, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDirectory, NSAccessibilityListRole},
      // If Mac supports AXExpandedChanged event with
      // NSAccessibilityDisclosureTriangleRole, We should update
      // ax::mojom::Role::kDisclosureTriangle mapping to
      // NSAccessibilityDisclosureTriangleRole. http://crbug.com/558324
      {ax::mojom::Role::kDisclosureTriangle, NSAccessibilityButtonRole},
      {ax::mojom::Role::kDocAbstract, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocAcknowledgments, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocAfterword, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocAppendix, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocBackLink, NSAccessibilityLinkRole},
      {ax::mojom::Role::kDocBiblioEntry, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocBibliography, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocBiblioRef, NSAccessibilityLinkRole},
      {ax::mojom::Role::kDocChapter, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocColophon, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocConclusion, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocCover, NSAccessibilityImageRole},
      {ax::mojom::Role::kDocCredit, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocCredits, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocDedication, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocEndnote, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocEndnotes, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocEpigraph, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocEpilogue, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocErrata, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocExample, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocFootnote, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocForeword, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocGlossary, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocGlossRef, NSAccessibilityLinkRole},
      {ax::mojom::Role::kDocIndex, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocIntroduction, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocNoteRef, NSAccessibilityLinkRole},
      {ax::mojom::Role::kDocNotice, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocPageBreak, NSAccessibilitySplitterRole},
      {ax::mojom::Role::kDocPageFooter, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocPageHeader, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocPageList, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocPart, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocPreface, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocPrologue, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocPullquote, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocQna, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocSubtitle, @"AXHeading"},
      {ax::mojom::Role::kDocTip, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocToc, NSAccessibilityGroupRole},
      {ax::mojom::Role::kDocument, NSAccessibilityGroupRole},
      {ax::mojom::Role::kEmbeddedObject, NSAccessibilityGroupRole},
      {ax::mojom::Role::kEmphasis, NSAccessibilityGroupRole},
      {ax::mojom::Role::kFeed, NSAccessibilityGroupRole},
      {ax::mojom::Role::kFigcaption, NSAccessibilityGroupRole},
      {ax::mojom::Role::kFigure, NSAccessibilityGroupRole},
      {ax::mojom::Role::kFooter, NSAccessibilityGroupRole},
      {ax::mojom::Role::kFooterAsNonLandmark, NSAccessibilityGroupRole},
      {ax::mojom::Role::kForm, NSAccessibilityGroupRole},
      {ax::mojom::Role::kGenericContainer, NSAccessibilityGroupRole},
      {ax::mojom::Role::kGraphicsDocument, NSAccessibilityGroupRole},
      {ax::mojom::Role::kGraphicsObject, NSAccessibilityGroupRole},
      {ax::mojom::Role::kGraphicsSymbol, NSAccessibilityImageRole},
      // Should be NSAccessibilityGridRole but VoiceOver treating it like
      // a list as of 10.12.6, so following WebKit and using table role:
      {ax::mojom::Role::kGrid, NSAccessibilityTableRole},  // crbug.com/753925
      {ax::mojom::Role::kGroup, NSAccessibilityGroupRole},
      {ax::mojom::Role::kHeader, NSAccessibilityGroupRole},
      {ax::mojom::Role::kHeaderAsNonLandmark, NSAccessibilityGroupRole},
      {ax::mojom::Role::kHeading, @"AXHeading"},
      {ax::mojom::Role::kIframe, NSAccessibilityGroupRole},
      {ax::mojom::Role::kIframePresentational, NSAccessibilityGroupRole},
      {ax::mojom::Role::kImage, NSAccessibilityImageRole},
      {ax::mojom::Role::kInputTime, @"AXTimeField"},
      {ax::mojom::Role::kLabelText, NSAccessibilityGroupRole},
      {ax::mojom::Role::kLayoutTable, NSAccessibilityGroupRole},
      {ax::mojom::Role::kLayoutTableCell, NSAccessibilityGroupRole},
      {ax::mojom::Role::kLayoutTableRow, NSAccessibilityGroupRole},
      {ax::mojom::Role::kLegend, NSAccessibilityGroupRole},
      {ax::mojom::Role::kLineBreak, NSAccessibilityGroupRole},
      {ax::mojom::Role::kLink, NSAccessibilityLinkRole},
      {ax::mojom::Role::kList, NSAccessibilityListRole},
      {ax::mojom::Role::kListBox, NSAccessibilityListRole},
      {ax::mojom::Role::kListBoxOption, NSAccessibilityStaticTextRole},
      {ax::mojom::Role::kListItem, NSAccessibilityGroupRole},
      {ax::mojom::Role::kListMarker, @"AXListMarker"},
      {ax::mojom::Role::kLog, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMain, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMark, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMarquee, NSAccessibilityGroupRole},
      // https://w3c.github.io/mathml-aam/#mathml-element-mappings
      {ax::mojom::Role::kMath, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLFraction, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLIdentifier, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLMath, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLMultiscripts, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLNoneScript, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLNumber, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLOperator, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLOver, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLPrescriptDelimiter, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLRoot, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLRow, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLSquareRoot, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLStringLiteral, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLSub, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLSubSup, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLSup, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLTable, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLTableCell, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLTableRow, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLText, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLUnder, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMathMLUnderOver, NSAccessibilityGroupRole},
      {ax::mojom::Role::kMenu, NSAccessibilityMenuRole},
      {ax::mojom::Role::kMenuBar, NSAccessibilityMenuBarRole},
      {ax::mojom::Role::kMenuItem, NSAccessibilityMenuItemRole},
      {ax::mojom::Role::kMenuItemCheckBox, NSAccessibilityMenuItemRole},
      {ax::mojom::Role::kMenuItemRadio, NSAccessibilityMenuItemRole},
      {ax::mojom::Role::kMenuListOption, NSAccessibilityMenuItemRole},
      {ax::mojom::Role::kMenuListPopup, NSAccessibilityMenuRole},
      {ax::mojom::Role::kMeter, NSAccessibilityLevelIndicatorRole},
      {ax::mojom::Role::kNavigation, NSAccessibilityGroupRole},
      {ax::mojom::Role::kNone, NSAccessibilityGroupRole},
      {ax::mojom::Role::kNote, NSAccessibilityGroupRole},
      {ax::mojom::Role::kParagraph, NSAccessibilityGroupRole},
      {ax::mojom::Role::kPdfActionableHighlight, NSAccessibilityButtonRole},
      {ax::mojom::Role::kPdfRoot, NSAccessibilityGroupRole},
      {ax::mojom::Role::kPluginObject, NSAccessibilityGroupRole},
      {ax::mojom::Role::kPopUpButton, NSAccessibilityPopUpButtonRole},
      {ax::mojom::Role::kPortal, NSAccessibilityButtonRole},
      {ax::mojom::Role::kPre, NSAccessibilityGroupRole},
      {ax::mojom::Role::kProgressIndicator,
       NSAccessibilityProgressIndicatorRole},
      {ax::mojom::Role::kRadioButton, NSAccessibilityRadioButtonRole},
      {ax::mojom::Role::kRadioGroup, NSAccessibilityRadioGroupRole},
      {ax::mojom::Role::kRegion, NSAccessibilityGroupRole},
      {ax::mojom::Role::kRootWebArea, NSAccessibilityWebAreaRole},
      {ax::mojom::Role::kRow, NSAccessibilityRowRole},
      {ax::mojom::Role::kRowGroup, NSAccessibilityGroupRole},
      {ax::mojom::Role::kRowHeader, @"AXCell"},
      {ax::mojom::Role::kRuby, NSAccessibilityGroupRole},
      {ax::mojom::Role::kRubyAnnotation, NSAccessibilityUnknownRole},
      {ax::mojom::Role::kScrollBar, NSAccessibilityScrollBarRole},
      {ax::mojom::Role::kSearch, NSAccessibilityGroupRole},
      {ax::mojom::Role::kSearchBox, NSAccessibilityTextFieldRole},
      {ax::mojom::Role::kSection, NSAccessibilityGroupRole},
      {ax::mojom::Role::kSlider, NSAccessibilitySliderRole},
      {ax::mojom::Role::kSpinButton, NSAccessibilityIncrementorRole},
      {ax::mojom::Role::kSplitter, NSAccessibilitySplitterRole},
      {ax::mojom::Role::kStaticText, NSAccessibilityStaticTextRole},
      {ax::mojom::Role::kStatus, NSAccessibilityGroupRole},
      {ax::mojom::Role::kSubscript, NSAccessibilityGroupRole},
      {ax::mojom::Role::kSuggestion, NSAccessibilityGroupRole},
      {ax::mojom::Role::kSuperscript, NSAccessibilityGroupRole},
      {ax::mojom::Role::kSvgRoot, NSAccessibilityGroupRole},
      {ax::mojom::Role::kSwitch, NSAccessibilityCheckBoxRole},
      {ax::mojom::Role::kStrong, NSAccessibilityGroupRole},
      {ax::mojom::Role::kTab, NSAccessibilityRadioButtonRole},
      {ax::mojom::Role::kTable, NSAccessibilityTableRole},
      {ax::mojom::Role::kTableHeaderContainer, NSAccessibilityGroupRole},
      {ax::mojom::Role::kTabList, NSAccessibilityTabGroupRole},
      {ax::mojom::Role::kTabPanel, NSAccessibilityGroupRole},
      {ax::mojom::Role::kTerm, NSAccessibilityGroupRole},
      {ax::mojom::Role::kTextField, NSAccessibilityTextFieldRole},
      {ax::mojom::Role::kTextFieldWithComboBox, NSAccessibilityComboBoxRole},
      {ax::mojom::Role::kTime, NSAccessibilityGroupRole},
      {ax::mojom::Role::kTimer, NSAccessibilityGroupRole},
      {ax::mojom::Role::kTitleBar, NSAccessibilityStaticTextRole},
      {ax::mojom::Role::kToggleButton, NSAccessibilityCheckBoxRole},
      {ax::mojom::Role::kToolbar, NSAccessibilityToolbarRole},
      {ax::mojom::Role::kTooltip, NSAccessibilityGroupRole},
      {ax::mojom::Role::kTree, NSAccessibilityOutlineRole},
      {ax::mojom::Role::kTreeGrid, NSAccessibilityTableRole},
      {ax::mojom::Role::kTreeItem, NSAccessibilityRowRole},
      {ax::mojom::Role::kVideo, NSAccessibilityGroupRole},
      // Use the group role as the BrowserNativeWidgetWindow already provides
      // a kWindow role, and having extra window roles, which are treated
      // specially by screen readers, can break their ability to find the
      // content window. See http://crbug.com/875843 for more information.
      {ax::mojom::Role::kWindow, NSAccessibilityGroupRole},
  };

  return RoleMap(begin(roles), end(roles));
}

RoleMap BuildSubroleMap() {
  const RoleMap::value_type subroles[] = {
      {ax::mojom::Role::kAlert, @"AXApplicationAlert"},
      {ax::mojom::Role::kAlertDialog, @"AXApplicationAlertDialog"},
      {ax::mojom::Role::kApplication, @"AXLandmarkApplication"},
      {ax::mojom::Role::kArticle, @"AXDocumentArticle"},
      {ax::mojom::Role::kBanner, @"AXLandmarkBanner"},
      {ax::mojom::Role::kCode, @"AXCodeStyleGroup"},
      {ax::mojom::Role::kComplementary, @"AXLandmarkComplementary"},
      {ax::mojom::Role::kContentDeletion, @"AXDeleteStyleGroup"},
      {ax::mojom::Role::kContentInsertion, @"AXInsertStyleGroup"},
      {ax::mojom::Role::kContentInfo, @"AXLandmarkContentInfo"},
      {ax::mojom::Role::kDefinition, @"AXDefinition"},
      {ax::mojom::Role::kDescriptionListDetail, @"AXDefinition"},
      {ax::mojom::Role::kDescriptionListTerm, @"AXTerm"},
      {ax::mojom::Role::kDialog, @"AXApplicationDialog"},
      {ax::mojom::Role::kDocument, @"AXDocument"},
      {ax::mojom::Role::kEmphasis, @"AXEmphasisStyleGroup"},
      {ax::mojom::Role::kFeed, @"AXApplicationGroup"},
      {ax::mojom::Role::kFooter, @"AXLandmarkContentInfo"},
      {ax::mojom::Role::kForm, @"AXLandmarkForm"},
      {ax::mojom::Role::kGraphicsDocument, @"AXDocument"},
      {ax::mojom::Role::kGroup, @"AXApplicationGroup"},
      {ax::mojom::Role::kHeader, @"AXLandmarkBanner"},
      {ax::mojom::Role::kLog, @"AXApplicationLog"},
      {ax::mojom::Role::kMain, @"AXLandmarkMain"},
      {ax::mojom::Role::kMarquee, @"AXApplicationMarquee"},
      // https://w3c.github.io/mathml-aam/#mathml-element-mappings
      {ax::mojom::Role::kMath, @"AXDocumentMath"},
      {ax::mojom::Role::kMathMLFraction, @"AXMathFraction"},
      {ax::mojom::Role::kMathMLIdentifier, @"AXMathIdentifier"},
      {ax::mojom::Role::kMathMLMath, @"AXDocumentMath"},
      {ax::mojom::Role::kMathMLMultiscripts, @"AXMathMultiscript"},
      {ax::mojom::Role::kMathMLNoneScript, @"AXMathRow"},
      {ax::mojom::Role::kMathMLNumber, @"AXMathNumber"},
      {ax::mojom::Role::kMathMLOperator, @"AXMathOperator"},
      {ax::mojom::Role::kMathMLOver, @"AXMathUnderOver"},
      {ax::mojom::Role::kMathMLPrescriptDelimiter, @"AXMathRow"},
      {ax::mojom::Role::kMathMLRoot, @"AXMathRoot"},
      {ax::mojom::Role::kMathMLRow, @"AXMathRow"},
      {ax::mojom::Role::kMathMLSquareRoot, @"AXMathSquareRoot"},
      {ax::mojom::Role::kMathMLSub, @"AXMathSubscriptSuperscript"},
      {ax::mojom::Role::kMathMLSubSup, @"AXMathSubscriptSuperscript"},
      {ax::mojom::Role::kMathMLSup, @"AXMathSubscriptSuperscript"},
      {ax::mojom::Role::kMathMLTable, @"AXMathTable"},
      {ax::mojom::Role::kMathMLTableCell, @"AXMathTableCell"},
      {ax::mojom::Role::kMathMLTableRow, @"AXMathTableRow"},
      {ax::mojom::Role::kMathMLText, @"AXMathText"},
      {ax::mojom::Role::kMathMLUnder, @"AXMathUnderOver"},
      {ax::mojom::Role::kMathMLUnderOver, @"AXMathUnderOver"},
      {ax::mojom::Role::kNavigation, @"AXLandmarkNavigation"},
      {ax::mojom::Role::kNote, @"AXDocumentNote"},
      {ax::mojom::Role::kRegion, @"AXLandmarkRegion"},
      {ax::mojom::Role::kSearch, @"AXLandmarkSearch"},
      {ax::mojom::Role::kSearchBox, @"AXSearchField"},
      {ax::mojom::Role::kStatus, @"AXApplicationStatus"},
      {ax::mojom::Role::kStrong, @"AXStrongStyleGroup"},
      {ax::mojom::Role::kSubscript, @"AXSubscriptStyleGroup"},
      {ax::mojom::Role::kSuperscript, @"AXSuperscriptStyleGroup"},
      {ax::mojom::Role::kSwitch, @"AXSwitch"},
      {ax::mojom::Role::kTabPanel, @"AXTabPanel"},
      {ax::mojom::Role::kTerm, @"AXTerm"},
      {ax::mojom::Role::kTime, @"AXTimeGroup"},
      {ax::mojom::Role::kTimer, @"AXApplicationTimer"},
      {ax::mojom::Role::kToggleButton, @"AXToggleButton"},
      {ax::mojom::Role::kTooltip, @"AXUserInterfaceTooltip"},
      {ax::mojom::Role::kTreeItem, NSAccessibilityOutlineRowSubrole},
  };

  return RoleMap(begin(subroles), end(subroles));
}

EventMap BuildEventMap() {
  const EventMap::value_type events[] = {
      {ax::mojom::Event::kCheckedStateChanged,
       NSAccessibilityValueChangedNotification},
      {ax::mojom::Event::kFocus,
       NSAccessibilityFocusedUIElementChangedNotification},
      {ax::mojom::Event::kFocusContext,
       NSAccessibilityFocusedUIElementChangedNotification},

      // Do not map kMenuStart/End to the Mac's opened/closed notifications.
      // kMenuStart/End are fired at the start/end of menu interaction on the
      // container of the menu; not the menu itself. All newly-opened/closed
      // menus should fire kMenuPopupStart/End. See SubmenuView::ShowAt and
      // SubmenuView::Hide.
      {ax::mojom::Event::kMenuPopupStart, (NSString*)kAXMenuOpenedNotification},
      {ax::mojom::Event::kMenuPopupEnd, (NSString*)kAXMenuClosedNotification},

      {ax::mojom::Event::kTextChanged, NSAccessibilityTitleChangedNotification},
      {ax::mojom::Event::kValueChanged,
       NSAccessibilityValueChangedNotification},
      {ax::mojom::Event::kTextSelectionChanged,
       NSAccessibilitySelectedTextChangedNotification},
      // TODO(patricialor): Add more events.
  };

  return EventMap(begin(events), end(events));
}

ActionList BuildActionList() {
  const ActionList::value_type entries[] = {
      // NSAccessibilityPressAction must come first in this list.
      {ax::mojom::Action::kDoDefault, NSAccessibilityPressAction},

      {ax::mojom::Action::kDecrement, NSAccessibilityDecrementAction},
      {ax::mojom::Action::kIncrement, NSAccessibilityIncrementAction},
      {ax::mojom::Action::kShowContextMenu, NSAccessibilityShowMenuAction},
  };
  return ActionList(begin(entries), end(entries));
}

const ActionList& GetActionList() {
  static const base::NoDestructor<ActionList> action_map(BuildActionList());
  return *action_map;
}

void PostAnnouncementNotification(NSString* announcement,
                                  NSWindow* window,
                                  bool is_polite) {
  NSAccessibilityPriorityLevel priority =
      is_polite ? NSAccessibilityPriorityMedium : NSAccessibilityPriorityHigh;
  NSDictionary* notification_info = @{
    NSAccessibilityAnnouncementKey : announcement,
    NSAccessibilityPriorityKey : @(priority)
  };
  // On Mojave, announcements from an inactive window aren't spoken.
  NSAccessibilityPostNotificationWithUserInfo(
      window, NSAccessibilityAnnouncementRequestedNotification,
      notification_info);
}

// Returns true if |action| should be added implicitly for |data|.
bool HasImplicitAction(const ui::AXPlatformNodeBase& node,
                       ax::mojom::Action action) {
  return action == ax::mojom::Action::kDoDefault &&
         node.GetData().IsClickable();
}

// For roles that show a menu for the default action, ensure "show menu" also
// appears in available actions, but only if that's not already used for a
// context menu. It will be mapped back to the default action when performed.
bool AlsoUseShowMenuActionForDefaultAction(const ui::AXPlatformNodeBase& node) {
  return HasImplicitAction(node, ax::mojom::Action::kDoDefault) &&
         !node.HasAction(ax::mojom::Action::kShowContextMenu) &&
         node.GetRole() == ax::mojom::Role::kPopUpButton;
}

// Check whether |selector| is an accessibility setter. This is a heuristic but
// seems to be a pretty good one.
bool IsAXSetter(SEL selector) {
  return [NSStringFromSelector(selector) hasPrefix:@"setAccessibility"];
}

}  // namespace

@interface AXPlatformNodeCocoa (Private)
// Helper function for string attributes that don't require extra processing.
- (NSString*)getStringAttribute:(ax::mojom::StringAttribute)attribute;

// Returns AXValue, or nil if AXValue isn't an NSString.
- (NSString*)getAXValueAsString;

// Returns this node's internal role, i.e. the one that is stored in
// the internal accessibility tree as opposed to the platform tree.
- (ax::mojom::Role)internalRole;

// Returns the native wrapper for the given node id.
- (AXPlatformNodeCocoa*)fromNodeID:(ui::AXNodeID)id;
@end

@implementation AXPlatformNodeCocoa {
  ui::AXPlatformNodeBase* _node;  // Weak. Retains us.
  std::unique_ptr<ui::AXAnnouncementSpec> _pendingAnnouncement;
}

@synthesize node = _node;

- (BOOL)instanceActive {
  return _node != nullptr;
}

+ (NSString*)nativeRoleFromAXRole:(ax::mojom::Role)role {
  static const base::NoDestructor<RoleMap> role_map(BuildRoleMap());
  RoleMap::const_iterator it = role_map->find(role);
  return it != role_map->end() ? it->second : NSAccessibilityUnknownRole;
}

+ (NSString*)nativeSubroleFromAXRole:(ax::mojom::Role)role {
  static const base::NoDestructor<RoleMap> subrole_map(BuildSubroleMap());
  RoleMap::const_iterator it = subrole_map->find(role);
  return it != subrole_map->end() ? it->second : nil;
}

+ (NSString*)nativeNotificationFromAXEvent:(ax::mojom::Event)event {
  static const base::NoDestructor<EventMap> event_map(BuildEventMap());
  EventMap::const_iterator it = event_map->find(event);
  return it != event_map->end() ? it->second : nil;
}

- (instancetype)initWithNode:(ui::AXPlatformNodeBase*)node {
  if ((self = [super init])) {
    _node = node;
  }
  return self;
}

- (void)detach {
  if (!_node)
    return;
  _node = nil;
  NSAccessibilityPostNotification(
      self, NSAccessibilityUIElementDestroyedNotification);
}

- (NSRect)boundsInScreen {
  if (!_node || !_node->GetDelegate())
    return NSZeroRect;
  return gfx::ScreenRectToNSRect(_node->GetDelegate()->GetBoundsRect(
      ui::AXCoordinateSystem::kScreenDIPs, ui::AXClippingBehavior::kClipped));
}

- (NSString*)getStringAttribute:(ax::mojom::StringAttribute)attribute {
  std::string attributeValue;
  if (_node->GetStringAttribute(attribute, &attributeValue))
    return base::SysUTF8ToNSString(attributeValue);
  return nil;
}

- (NSString*)getAXValueAsString {
  id value = [self AXValue];
  return [value isKindOfClass:[NSString class]] ? value : nil;
}

- (ax::mojom::Role)internalRole {
  if ([self instanceActive])
    return _node->GetRole();
  return ax::mojom::Role::kUnknown;
}

- (AXPlatformNodeCocoa*)fromNodeID:(ui::AXNodeID)id {
  ui::AXPlatformNode* cell = _node->GetDelegate()->GetFromNodeID(id);
  if (cell)
    return cell->GetNativeViewAccessible();
  return nil;
}

- (NSString*)getName {
  return base::SysUTF8ToNSString(_node->GetName());
}

- (std::unique_ptr<ui::AXAnnouncementSpec>)announcementForEvent:
    (ax::mojom::Event)eventType {
  // Only alerts and live region changes should be announced.
  DCHECK(eventType == ax::mojom::Event::kAlert ||
         eventType == ax::mojom::Event::kLiveRegionChanged);
  std::string liveStatus =
      _node->GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus);
  // If live status is explicitly set to off, don't announce.
  if (liveStatus == "off")
    return nullptr;

  NSString* name = [self getName];
  NSString* announcementText =
      [name length] > 0
          ? name
          : base::SysUTF16ToNSString(_node->GetTextContentUTF16());
  if ([announcementText length] == 0)
    return nullptr;

  const std::string& description =
      _node->GetStringAttribute(ax::mojom::StringAttribute::kDescription);
  if (!description.empty()) {
    // Concatenating name and description, with a newline in between to create a
    // pause to avoid treating the concatenation as a single sentence.
    announcementText =
        [NSString stringWithFormat:@"%@\n%@", announcementText,
                                   base::SysUTF8ToNSString(description)];
  }

  auto announcement = std::make_unique<ui::AXAnnouncementSpec>();
  announcement->announcement =
      base::scoped_nsobject<NSString>([announcementText retain]);
  announcement->window =
      base::scoped_nsobject<NSWindow>([[self AXWindow] retain]);
  announcement->is_polite = liveStatus != "assertive";
  return announcement;
}

- (void)scheduleLiveRegionAnnouncement:
    (std::unique_ptr<ui::AXAnnouncementSpec>)announcement {
  if (_pendingAnnouncement) {
    // An announcement is already in flight, so just reset the contents. This is
    // threadsafe because the dispatch is on the main queue.
    _pendingAnnouncement = std::move(announcement);
    return;
  }

  _pendingAnnouncement = std::move(announcement);
  dispatch_after(
      kLiveRegionDebounceMillis * NSEC_PER_MSEC, dispatch_get_main_queue(), ^{
        if (!_pendingAnnouncement) {
          return;
        }
        PostAnnouncementNotification(_pendingAnnouncement->announcement,
                                     _pendingAnnouncement->window,
                                     _pendingAnnouncement->is_polite);
        _pendingAnnouncement.reset();
      });
}

// NSAccessibility informal protocol implementation.

- (BOOL)accessibilityIsIgnored {
  return ![self isAccessibilityElement];
}

- (id)accessibilityHitTest:(NSPoint)point {
  if (!NSPointInRect(point, [self boundsInScreen]))
    return nil;

  for (id child in [[self AXChildren] reverseObjectEnumerator]) {
    if (!NSPointInRect(point, [child accessibilityFrame]))
      continue;
    if (id foundChild = [child accessibilityHitTest:point])
      return foundChild;
  }

  // Hit self, but not any child.
  return NSAccessibilityUnignoredAncestor(self);
}

- (BOOL)accessibilityNotifiesWhenDestroyed {
  return YES;
}

- (id)accessibilityFocusedUIElement {
  return _node ? _node->GetDelegate()->GetFocus() : nil;
}

// This function and accessibilityPerformAction:, while deprecated, are a) still
// called by AppKit internally and b) not implemented by NSAccessibilityElement,
// so this class needs its own implementations.
- (NSArray*)accessibilityActionNames {
  if (!_node)
    return @[];

  base::scoped_nsobject<NSMutableArray> axActions(
      [[NSMutableArray alloc] init]);
  const ActionList& action_list = GetActionList();

  // VoiceOver expects the "press" action to be first. Note that some roles
  // should be given a press action implicitly.
  DCHECK([action_list[0].second isEqualToString:NSAccessibilityPressAction]);
  for (const auto& item : action_list) {
    if (_node->HasAction(item.first) || HasImplicitAction(*_node, item.first))
      [axActions addObject:item.second];
  }

  if (AlsoUseShowMenuActionForDefaultAction(*_node))
    [axActions addObject:NSAccessibilityShowMenuAction];

  return axActions.autorelease();
}

- (void)accessibilityPerformAction:(NSString*)action {
  // Actions are performed asynchronously, so it's always possible for an object
  // to change its mind after previously reporting an action as available.
  if (![[self accessibilityActionNames] containsObject:action])
    return;

  ui::AXActionData data;
  if ([action isEqualToString:NSAccessibilityShowMenuAction] &&
      AlsoUseShowMenuActionForDefaultAction(*_node)) {
    data.action = ax::mojom::Action::kDoDefault;
  } else {
    for (const ActionList::value_type& entry : GetActionList()) {
      if ([action isEqualToString:entry.second]) {
        data.action = entry.first;
        break;
      }
    }
  }

  // Note ui::AX_ACTIONs which are just overwriting an accessibility attribute
  // are already implemented in -accessibilitySetValue:forAttribute:, so ignore
  // those here.

  if (data.action != ax::mojom::Action::kNone)
    _node->GetDelegate()->AccessibilityPerformAction(data);
}

// This method, while deprecated, is still called internally by AppKit.
- (NSArray*)accessibilityAttributeNames {
  if (!_node)
    return @[];

  // These attributes are required on all accessibility objects.
  NSArray* const kAllRoleAttributes = @[
    NSAccessibilityDOMIdentifierAttribute,
    NSAccessibilityChildrenAttribute,
    NSAccessibilityParentAttribute,
    NSAccessibilityPositionAttribute,
    NSAccessibilityRoleAttribute,
    NSAccessibilitySizeAttribute,
    NSAccessibilitySubroleAttribute,
    // Title is required for most elements. Cocoa asks for the value even if it
    // is omitted here, but won't present it to accessibility APIs without this.
    NSAccessibilityTitleAttribute,
    // Attributes which are not required, but are general to all roles.
    NSAccessibilityRoleDescriptionAttribute,
    NSAccessibilityEnabledAttribute,
    NSAccessibilityFocusedAttribute,
    NSAccessibilityHelpAttribute,
    NSAccessibilityTopLevelUIElementAttribute,
    NSAccessibilityWindowAttribute,
  ];
  // Attributes required for user-editable controls.
  NSArray* const kValueAttributes = @[ NSAccessibilityValueAttribute ];
  // Attributes required for unprotected textfields and labels.
  NSArray* const kUnprotectedTextAttributes = @[
    NSAccessibilityInsertionPointLineNumberAttribute,
    NSAccessibilityNumberOfCharactersAttribute,
    NSAccessibilitySelectedTextAttribute,
    NSAccessibilitySelectedTextRangeAttribute,
    NSAccessibilityVisibleCharacterRangeAttribute,
  ];
  // Required for all text, including protected textfields.
  NSString* const kTextAttributes = NSAccessibilityPlaceholderValueAttribute;
  base::scoped_nsobject<NSMutableArray> axAttributes(
      [[NSMutableArray alloc] init]);
  [axAttributes addObjectsFromArray:kAllRoleAttributes];

  ax::mojom::Role role = _node->GetRole();
  switch (role) {
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kStaticText:
      [axAttributes addObject:kTextAttributes];
      if (!_node->HasState(ax::mojom::State::kProtected))
        [axAttributes addObjectsFromArray:kUnprotectedTextAttributes];
      FALLTHROUGH;
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kSearchBox:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kToggleButton:
      [axAttributes addObjectsFromArray:kValueAttributes];
      break;
    case ax::mojom::Role::kMathMLFraction:
      [axAttributes addObjectsFromArray:@[
        NSAccessibilityMathFractionNumeratorAttribute,
        NSAccessibilityMathFractionDenominatorAttribute
      ]];
      break;
    case ax::mojom::Role::kMathMLSquareRoot:
      [axAttributes addObject:NSAccessibilityMathRootRadicandAttribute];
      break;
    case ax::mojom::Role::kMathMLRoot:
      [axAttributes addObjectsFromArray:@[
        NSAccessibilityMathRootRadicandAttribute,
        NSAccessibilityMathRootIndexAttribute
      ]];
      break;
    case ax::mojom::Role::kMathMLSub:
      [axAttributes addObjectsFromArray:@[
        NSAccessibilityMathBaseAttribute, NSAccessibilityMathSubscriptAttribute
      ]];
      break;
    case ax::mojom::Role::kMathMLSup:
      [axAttributes addObjectsFromArray:@[
        NSAccessibilityMathBaseAttribute,
        NSAccessibilityMathSuperscriptAttribute
      ]];
      break;
    case ax::mojom::Role::kMathMLSubSup:
      [axAttributes addObjectsFromArray:@[
        NSAccessibilityMathBaseAttribute, NSAccessibilityMathSubscriptAttribute,
        NSAccessibilityMathSuperscriptAttribute
      ]];
      break;
    case ax::mojom::Role::kMathMLUnder:
      [axAttributes addObjectsFromArray:@[
        NSAccessibilityMathBaseAttribute, NSAccessibilityMathUnderAttribute
      ]];
      break;
    case ax::mojom::Role::kMathMLOver:
      [axAttributes addObjectsFromArray:@[
        NSAccessibilityMathBaseAttribute, NSAccessibilityMathOverAttribute
      ]];
      break;
    case ax::mojom::Role::kMathMLUnderOver:
      [axAttributes addObjectsFromArray:@[
        NSAccessibilityMathBaseAttribute, NSAccessibilityMathUnderAttribute,
        NSAccessibilityMathOverAttribute
      ]];
      break;
    case ax::mojom::Role::kMathMLMultiscripts:
      [axAttributes addObjectsFromArray:@[
        NSAccessibilityMathBaseAttribute,
        NSAccessibilityMathPostscriptsAttribute,
        NSAccessibilityMathPrescriptsAttribute
      ]];
      break;
      // TODO(tapted): Add additional attributes based on role.
    default:
      break;
  }
  if (_node->HasBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    [axAttributes addObject:NSAccessibilitySelectedAttribute];
  if (ui::IsMenuItem(role))
    [axAttributes addObject:@"AXMenuItemMarkChar"];
  if (ui::IsItemLike(role))
    [axAttributes addObjectsFromArray:@[ @"AXARIAPosInSet", @"AXARIASetSize" ]];
  if (ui::IsSetLike(role))
    [axAttributes addObject:@"AXARIASetSize"];

  // Live regions.
  if (_node->HasStringAttribute(ax::mojom::StringAttribute::kLiveStatus))
    [axAttributes addObject:NSAccessibilityARIALiveAttribute];
  if (_node->HasStringAttribute(ax::mojom::StringAttribute::kLiveRelevant))
    [axAttributes addObject:NSAccessibilityARIARelevantAttribute];
  if (_node->HasBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic))
    [axAttributes addObject:NSAccessibilityARIAAtomicAttribute];
  if (_node->HasBoolAttribute(ax::mojom::BoolAttribute::kBusy))
    [axAttributes addObject:NSAccessibilityARIABusyAttribute];
  if (_node->HasIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState))
    [axAttributes addObject:NSAccessibilityARIACurrentAttribute];

  // Control element.
  if (ui::IsControl(role)) {
    [axAttributes addObjectsFromArray:@[
      NSAccessibilityAccessKeyAttribute,
      NSAccessibilityInvalidAttribute,
    ]];
  }

  // Autocomplete.
  if (_node->HasStringAttribute(ax::mojom::StringAttribute::kAutoComplete))
    [axAttributes addObject:NSAccessibilityAutocompleteValueAttribute];

  // Details.
  if (_node->HasIntListAttribute(ax::mojom::IntListAttribute::kDetailsIds)) {
    [axAttributes addObject:NSAccessibilityDetailsElementsAttribute];
  }

  if (ui::SupportsRequired(role)) {
    [axAttributes addObject:NSAccessibilityRequiredAttributeChrome];
  }

  // Url: add the url attribute only if the object has a valid url.
  if ([self accessibilityURL])
    [axAttributes addObject:NSAccessibilityURLAttribute];

  // Table and grid.
  if (ui::IsTableLike(role)) {
    [axAttributes addObjectsFromArray:@[
      NSAccessibilityColumnHeaderUIElementsAttribute,
      NSAccessibilityARIAColumnCountAttribute,
      NSAccessibilityARIARowCountAttribute,
    ]];
  }
  if (ui::IsCellOrTableHeader(role)) {
    [axAttributes addObjectsFromArray:@[
      NSAccessibilityARIAColumnIndexAttribute,
      NSAccessibilityARIARowIndexAttribute,
    ]];
  }
  if (ui::IsCellOrTableHeader(role) && role != ax::mojom::Role::kColumnHeader) {
    [axAttributes addObject:NSAccessibilityColumnHeaderUIElementsAttribute];
  }

  // Popup
  if (_node->HasIntAttribute(ax::mojom::IntAttribute::kHasPopup)) {
    [axAttributes addObjectsFromArray:@[
      NSAccessibilityHasPopupAttribute, NSAccessibilityPopupValueAttribute
    ]];
  }

  return axAttributes.autorelease();
}

// Despite it being deprecated, AppKit internally calls this function sometimes
// in unclear circumstances. It is implemented in terms of the new a11y API
// here.
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  if (!_node)
    return;

  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    [self setAccessibilityValue:value];
  } else if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute]) {
    [self setAccessibilitySelectedText:base::mac::ObjCCastStrict<NSString>(
                                           value)];
  } else if ([attribute
                 isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
    [self setAccessibilitySelectedTextRange:base::mac::ObjCCastStrict<NSValue>(
                                                value)
                                                .rangeValue];
  } else if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    [self setAccessibilityFocused:base::mac::ObjCCastStrict<NSNumber>(value)
                                      .boolValue];
  }
}

// This method, while deprecated, is still called internally by AppKit.
- (id)accessibilityAttributeValue:(NSString*)attribute {
  if (!_node)
    return nil;  // Return nil when detached. Even for ax::mojom::Role.

  SEL selector = NSSelectorFromString(attribute);
  if ([self respondsToSelector:selector])
    return [self performSelector:selector];
  return nil;
}

- (id)accessibilityAttributeValue:(NSString*)attribute
                     forParameter:(id)parameter {
  if (!_node)
    return nil;

  SEL selector = NSSelectorFromString([attribute stringByAppendingString:@":"]);
  if ([self respondsToSelector:selector])
    return [self performSelector:selector withObject:parameter];
  return nil;
}

// NSAccessibility attributes. Order them according to
// NSAccessibilityConstants.h, or see https://crbug.com/678898.

- (NSString*)AXAccessKey {
  if (![self instanceActive])
    return nil;

  return [self getStringAttribute:ax::mojom::StringAttribute::kAccessKey];
}

- (NSNumber*)AXARIAAtomic {
  if (![self instanceActive])
    return nil;

  return @(_node->GetBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic));
}

- (NSNumber*)AXARIABusy {
  if (![self instanceActive])
    return nil;

  return @(_node->GetBoolAttribute(ax::mojom::BoolAttribute::kBusy));
}

- (NSString*)AXARIACurrent {
  if (![self instanceActive])
    return nil;

  int ariaCurrent;
  if (!_node->GetIntAttribute(ax::mojom::IntAttribute::kAriaCurrentState,
                              &ariaCurrent))
    return nil;

  switch (static_cast<ax::mojom::AriaCurrentState>(ariaCurrent)) {
    case ax::mojom::AriaCurrentState::kNone:
      NOTREACHED();
      return @"false";
    case ax::mojom::AriaCurrentState::kFalse:
      return @"false";
    case ax::mojom::AriaCurrentState::kTrue:
      return @"true";
    case ax::mojom::AriaCurrentState::kPage:
      return @"page";
    case ax::mojom::AriaCurrentState::kStep:
      return @"step";
    case ax::mojom::AriaCurrentState::kLocation:
      return @"location";
    case ax::mojom::AriaCurrentState::kDate:
      return @"date";
    case ax::mojom::AriaCurrentState::kTime:
      return @"time";
  }

  NOTREACHED();
  return @"false";
}

- (NSNumber*)AXARIAColumnCount {
  if (![self instanceActive])
    return nil;
  absl::optional<int> ariaColCount =
      _node->GetDelegate()->GetTableAriaColCount();
  if (!ariaColCount)
    return nil;
  return @(*ariaColCount);
}

- (NSNumber*)AXARIAColumnIndex {
  if (![self instanceActive])
    return nil;

  absl::optional<int> ariaColIndex =
      _node->GetDelegate()->GetTableCellAriaColIndex();
  if (!ariaColIndex)

    return nil;
  return @(*ariaColIndex);
}

- (NSString*)AXARIALive {
  if (![self instanceActive])
    return nil;

  return [self getStringAttribute:ax::mojom::StringAttribute::kLiveStatus];
}

- (NSString*)AXARIARelevant {
  if (![self instanceActive])
    return nil;

  return [self getStringAttribute:ax::mojom::StringAttribute::kLiveRelevant];
}

- (NSNumber*)AXARIARowCount {
  if (![self instanceActive])
    return nil;
  absl::optional<int> ariaRowCount =
      _node->GetDelegate()->GetTableAriaRowCount();
  if (!ariaRowCount)
    return nil;
  return @(*ariaRowCount);
}

- (NSNumber*)AXARIARowIndex {
  if (![self instanceActive])
    return nil;
  absl::optional<int> ariaRowIndex =
      _node->GetDelegate()->GetTableCellAriaRowIndex();
  if (!ariaRowIndex)
    return nil;
  return @(*ariaRowIndex);
}

- (NSString*)AXAutocompleteValue {
  if (![self instanceActive])
    return nil;

  return [self getStringAttribute:ax::mojom::StringAttribute::kAutoComplete];
}

- (NSArray*)AXColumnHeaderUIElements {
  return [self accessibilityColumnHeaderUIElements];
}

- (NSArray*)AXDetailsElements {
  if (![self instanceActive])
    return nil;

  NSMutableArray* elements = [[[NSMutableArray alloc] init] autorelease];
  for (ui::AXNodeID id :
       _node->GetIntListAttribute(ax::mojom::IntListAttribute::kDetailsIds)) {
    AXPlatformNodeCocoa* node = [self fromNodeID:id];
    if (node)
      [elements addObject:node];
  }

  return [elements count] ? elements : nil;
}

- (NSString*)AXDOMIdentifier {
  if (![self instanceActive])
    return nil;

  std::string id;
  if (_node->GetHtmlAttribute("id", &id))
    return base::SysUTF8ToNSString(id);

  return @"";
}

- (NSNumber*)AXHasPopup {
  if (![self instanceActive])
    return nil;
  return @(_node->HasIntAttribute(ax::mojom::IntAttribute::kHasPopup));
}

- (NSString*)AXInvalid {
  if (![self instanceActive])
    return nil;
  switch (_node->GetData().GetInvalidState()) {
    case ax::mojom::InvalidState::kNone:
    case ax::mojom::InvalidState::kFalse:
      return @"false";
    case ax::mojom::InvalidState::kTrue:
      return @"true";
  }
}

- (NSString*)AXPopupValue {
  if (![self instanceActive])
    return nil;
  int hasPopup = _node->GetIntAttribute(ax::mojom::IntAttribute::kHasPopup);
  switch (static_cast<ax::mojom::HasPopup>(hasPopup)) {
    case ax::mojom::HasPopup::kFalse:
      return @"false";
    case ax::mojom::HasPopup::kTrue:
      return @"true";
    case ax::mojom::HasPopup::kMenu:
      return @"menu";
    case ax::mojom::HasPopup::kListbox:
      return @"listbox";
    case ax::mojom::HasPopup::kTree:
      return @"tree";
    case ax::mojom::HasPopup::kGrid:
      return @"grid";
    case ax::mojom::HasPopup::kDialog:
      return @"dialog";
  }
}

- (NSNumber*)AXRequired {
  return [self accessibilityRequired];
}

- (NSString*)AXRole {
  if (!_node)
    return nil;

  return [[self class] nativeRoleFromAXRole:_node->GetRole()];
}

- (NSString*)AXRoleDescription {
  return [self accessibilityRoleDescription];
}

- (NSString*)AXSubrole {
  ax::mojom::Role role = _node->GetRole();
  switch (role) {
    case ax::mojom::Role::kTextField:
      if (_node->HasState(ax::mojom::State::kProtected))
        return NSAccessibilitySecureTextFieldSubrole;
      break;
    default:
      break;
  }
  return [AXPlatformNodeCocoa nativeSubroleFromAXRole:role];
}

- (NSURL*)AXURL {
  return [self accessibilityURL];
}

- (NSString*)AXHelp {
  // TODO(aleventhal) Key shortcuts attribute should eventually get
  // its own field. Follow what WebKit does for aria-keyshortcuts, see
  // https://bugs.webkit.org/show_bug.cgi?id=159215 (WebKit bug).
  NSString* desc =
      [self getStringAttribute:ax::mojom::StringAttribute::kDescription];
  NSString* key =
      [self getStringAttribute:ax::mojom::StringAttribute::kKeyShortcuts];
  if (!desc.length)
    return key.length ? key : @"";
  if (!key.length)
    return desc;
  return [NSString stringWithFormat:@"%@ %@", desc, key];
}

- (id)AXValue {
  ax::mojom::Role role = _node->GetRole();
  if (role == ax::mojom::Role::kTab)
    return [self AXSelected];

  if (ui::IsNameExposedInAXValueForRole(role))
    return [self getName];

  if (_node->IsPlatformCheckable()) {
    // Mixed checkbox state not currently supported in views, but could be.
    // See browser_accessibility_cocoa.mm for details.
    const auto checkedState = static_cast<ax::mojom::CheckedState>(
        _node->GetIntAttribute(ax::mojom::IntAttribute::kCheckedState));
    return checkedState == ax::mojom::CheckedState::kTrue ? @1 : @0;
  }
  return base::SysUTF16ToNSString(_node->GetValueForControl());
}

- (NSNumber*)AXEnabled {
  return
      @(_node->GetData().GetRestriction() != ax::mojom::Restriction::kDisabled);
}

- (NSNumber*)AXFocused {
  if (_node->HasState(ax::mojom::State::kFocusable))
    return
        @(_node->GetDelegate()->GetFocus() == _node->GetNativeViewAccessible());
  return @NO;
}

- (id)AXParent {
  if (!_node)
    return nil;
  return NSAccessibilityUnignoredAncestor(_node->GetParent());
}

- (NSArray*)AXChildren {
  if (!_node)
    return @[];

  int count = _node->GetChildCount();
  NSMutableArray* children = [NSMutableArray arrayWithCapacity:count];
  for (auto child_iterator_ptr = _node->GetDelegate()->ChildrenBegin();
       *child_iterator_ptr != *_node->GetDelegate()->ChildrenEnd();
       ++(*child_iterator_ptr)) {
    [children addObject:child_iterator_ptr->GetNativeViewAccessible()];
  }
  return NSAccessibilityUnignoredChildren(children);
}

- (id)AXWindow {
  return _node->GetDelegate()->GetNSWindow();
}

- (id)AXTopLevelUIElement {
  return [self AXWindow];
}

- (NSValue*)AXPosition {
  return [NSValue valueWithPoint:self.boundsInScreen.origin];
}

- (NSValue*)AXSize {
  return [NSValue valueWithSize:self.boundsInScreen.size];
}

- (NSString*)AXTitle {
  if (ui::IsNameExposedInAXValueForRole(_node->GetRole()))
    return @"";

  return [self getName];
}

- (NSString*)AXDescription {
  return [self AXTitle];
}

// Misc attributes.

- (NSNumber*)AXSelected {
  return @(_node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

- (NSString*)AXPlaceholderValue {
  return [self accessibilityPlaceholderValue];
}

- (NSString*)AXMenuItemMarkChar {
  if (!ui::IsMenuItem(_node->GetRole()))
    return nil;

  const auto checkedState = static_cast<ax::mojom::CheckedState>(
      _node->GetIntAttribute(ax::mojom::IntAttribute::kCheckedState));
  if (checkedState == ax::mojom::CheckedState::kTrue) {
    return @"\xE2\x9C\x93";  // UTF-8 for unicode 0x2713, "check mark"
  }

  return @"";
}

- (NSNumber*)AXARIAPosInSet {
  if (![self instanceActive])
    return nil;
  absl::optional<int> posInSet = _node->GetPosInSet();
  if (!posInSet)
    return nil;
  return @(*posInSet);
}

- (NSNumber*)AXARIASetSize {
  if (![self instanceActive])
    return nil;
  absl::optional<int> setSize = _node->GetSetSize();
  if (!setSize)
    return nil;
  return @(*setSize);
}

// Text-specific attributes.

- (NSString*)AXSelectedText {
  NSRange selectedTextRange;
  [[self AXSelectedTextRange] getValue:&selectedTextRange];
  return [[self getAXValueAsString] substringWithRange:selectedTextRange];
}

- (NSValue*)AXSelectedTextRange {
  int start = 0, end = 0;
  if (_node->IsAtomicTextField() &&
      _node->GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart, &start) &&
      _node->GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, &end)) {
    // NSRange cannot represent the direction the text was selected in.
    return
        [NSValue valueWithRange:{static_cast<NSUInteger>(std::min(start, end)),
                                 static_cast<NSUInteger>(abs(end - start))}];
  }

  return [NSValue valueWithRange:NSMakeRange(0, 0)];
}

- (NSNumber*)AXNumberOfCharacters {
  return @([[self getAXValueAsString] length]);
}

- (NSValue*)AXVisibleCharacterRange {
  return [NSValue valueWithRange:{0, [[self getAXValueAsString] length]}];
}

- (NSNumber*)AXInsertionPointLineNumber {
  // TODO: multiline is not supported on views.
  return @0;
}

// Parameterized text-specific attributes.

- (id)AXLineForIndex:(id)parameter {
  // TODO: multiline is not supported on views.
  return @0;
}

- (id)AXRangeForLine:(id)parameter {
  if (![parameter isKindOfClass:[NSNumber class]] || [parameter intValue] != 0)
    return nil;

  return [NSValue valueWithRange:{0, [[self getAXValueAsString] length]}];
}

- (id)AXStringForRange:(id)parameter {
  if (![parameter isKindOfClass:[NSValue class]] ||
      (0 != strcmp([parameter objCType], @encode(NSRange))))
    return nil;

  return [[self getAXValueAsString] substringWithRange:[parameter rangeValue]];
}

- (id)AXRangeForPosition:(id)parameter {
  // TODO(tapted): Hit-test [parameter pointValue] and return an NSRange.
  NOTIMPLEMENTED();
  return nil;
}

- (id)AXRangeForIndex:(id)parameter {
  NOTIMPLEMENTED();
  return nil;
}

- (id)AXBoundsForRange:(id)parameter {
  // TODO(tapted): Provide an accessor on AXPlatformNodeDelegate to obtain this
  // from ui::TextInputClient::GetCompositionCharacterBounds().
  NOTIMPLEMENTED();
  return nil;
}

- (id)AXRTFForRange:(id)parameter {
  NOTIMPLEMENTED();
  return nil;
}

- (id)AXStyleRangeForIndex:(id)parameter {
  if (![parameter isKindOfClass:[NSNumber class]])
    return nil;

  // TODO(https://crbug.com/958811): Implement this for real.
  return [NSValue
      valueWithRange:NSMakeRange(0, [self accessibilityNumberOfCharacters])];
}

- (id)AXAttributedStringForRange:(id)parameter {
  if (![parameter isKindOfClass:[NSValue class]])
    return nil;

  // TODO(https://crbug.com/958811): Implement this for real.
  base::scoped_nsobject<NSAttributedString> attributedString(
      [[NSAttributedString alloc]
          initWithString:[self AXStringForRange:parameter]]);
  // TODO(tapted): views::WordLookupClient has a way to obtain the actual
  // decorations, and BridgedContentView has a conversion function that creates
  // an NSAttributedString. Refactor things so they can be used here.
  return attributedString.autorelease();
}

- (NSString*)description {
  return [NSString stringWithFormat:@"%@ - %@ (%@)", [super description],
                                    [self AXTitle], [self AXRole]];
}

// The methods below implement the NSAccessibility protocol. These methods
// appear to be the minimum needed to avoid AppKit refusing to handle the
// element or crashing internally. Most of the remaining old API methods (the
// ones from NSObject) are implemented in terms of the new NSAccessibility
// methods.
//
// TODO(https://crbug.com/386671): Does this class need to implement the various
// accessibilityPerformFoo methods, or are the stub implementations from
// NSAccessibilityElement sufficient?
- (NSArray*)accessibilityChildren {
  return [self AXChildren];
}

- (BOOL)isAccessibilityElement {
  if (!_node)
    return NO;

  return (![[self AXRole] isEqualToString:NSAccessibilityUnknownRole] &&
          !_node->IsInvisibleOrIgnored());
}
- (BOOL)isAccessibilityEnabled {
  if (!_node)
    return NO;

  return _node->GetData().GetRestriction() != ax::mojom::Restriction::kDisabled;
}
- (NSRect)accessibilityFrame {
  return [self boundsInScreen];
}

- (NSString*)accessibilityLabel {
  // accessibilityLabel is "a short description of the accessibility element",
  // and accessibilityTitle is "the title of the accessibility element"; at
  // least in Chromium, the title usually is a short description of the element,
  // so it also functions as a label.
  return [self AXDescription];
}

- (NSString*)accessibilityTitle {
  return [self AXTitle];
}

- (id)accessibilityValue {
  return [self AXValue];
}

// NSAccessibility protocol:
- (NSNumber*)accessibilityRequired {
  TRACE_EVENT1("accessibility", "accessibilityRequired",
               "role=", ui::ToString([self internalRole]));

  if (![self instanceActive])
    return nil;

  return _node->HasState(ax::mojom::State::kRequired) ? @YES : @NO;
}

- (NSAccessibilityRole)accessibilityRole {
  return [self AXRole];
}

- (NSAccessibilitySubrole)accessibilitySubrole {
  return [self AXSubrole];
}

- (BOOL)isAccessibilitySelectorAllowed:(SEL)selector {
  TRACE_EVENT1(
      "accessibility", "AXPlatformNodeCocoa::isAccessibilitySelectorAllowed",
      "selector=", base::SysNSStringToUTF8(NSStringFromSelector(selector)));

  if (!_node)
    return NO;

  if (selector == @selector(setAccessibilityFocused:))
    return _node->HasState(ax::mojom::State::kFocusable);

  if (selector == @selector(setAccessibilityValue:) &&
      _node->GetRole() == ax::mojom::Role::kTab) {
    // Tabs use the radio button role on Mac, so they are selected by calling
    // setSelected on an individual tab, rather than by setting the selected
    // element on the tabstrip as a whole.
    return !_node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
  }

  // Don't allow calling AX setters on disabled elements.
  // TODO(https://crbug.com/692362): Once the underlying bug in
  // views::Textfield::SetSelectionRange() described in that bug is fixed,
  // remove the check here when the selector is setAccessibilitySelectedText*;
  // right now, this check serves to prevent accessibility clients from trying
  // to set the selection range, which won't work because of 692362.
  if (_node->GetData().IsReadOnlyOrDisabled() && IsAXSetter(selector))
    return NO;

  // TODO(https://crbug.com/386671): What about role-specific selectors?
  return [super isAccessibilitySelectorAllowed:selector];
}

- (void)setAccessibilityValue:(id)value {
  if (!_node)
    return;

  ui::AXActionData data;
  data.action = _node->GetRole() == ax::mojom::Role::kTab
                    ? ax::mojom::Action::kSetSelection
                    : ax::mojom::Action::kSetValue;
  if ([value isKindOfClass:[NSString class]]) {
    data.value = base::SysNSStringToUTF8(value);
  } else if ([value isKindOfClass:[NSValue class]]) {
    // TODO(https://crbug.com/386671): Is this case actually needed? The
    // NSObject accessibility implementation supported this, but can it actually
    // occur?
    NSRange range = [value rangeValue];
    data.anchor_offset = range.location;
    data.focus_offset = NSMaxRange(range);
  }
  _node->GetDelegate()->AccessibilityPerformAction(data);
}

- (void)setAccessibilityFocused:(BOOL)isFocused {
  if (!_node)
    return;

  ui::AXActionData data;
  data.action =
      isFocused ? ax::mojom::Action::kFocus : ax::mojom::Action::kBlur;
  _node->GetDelegate()->AccessibilityPerformAction(data);
}

- (void)setAccessibilitySelectedText:(NSString*)text {
  if (!_node)
    return;

  ui::AXActionData data;
  data.action = ax::mojom::Action::kReplaceSelectedText;
  data.value = base::SysNSStringToUTF8(text);

  _node->GetDelegate()->AccessibilityPerformAction(data);
}

- (void)setAccessibilitySelectedTextRange:(NSRange)range {
  if (!_node)
    return;

  ui::AXActionData data;
  data.action = ax::mojom::Action::kSetSelection;
  data.anchor_offset = range.location;
  data.anchor_node_id = _node->GetData().id;
  data.focus_offset = NSMaxRange(range);
  data.focus_node_id = _node->GetData().id;
  _node->GetDelegate()->AccessibilityPerformAction(data);
}

// "Configuring Text Elements" section of the NSAccessibility formal protocol.
// These are all "required" methods, although in practice the ones that are left
// NOTIMPLEMENTED() seem to not be called anywhere (and were NOTIMPLEMENTED in
// the old API as well).

- (NSInteger)accessibilityInsertionPointLineNumber {
  return [[self AXInsertionPointLineNumber] integerValue];
}

- (NSInteger)accessibilityNumberOfCharacters {
  if (!_node)
    return 0;

  return [[self AXNumberOfCharacters] integerValue];
}

- (NSString*)accessibilityPlaceholderValue {
  if (![self instanceActive])
    return nil;

  if (_node->GetNameFrom() == ax::mojom::NameFrom::kPlaceholder)
    return base::SysUTF8ToNSString(_node->GetName());

  return [self getStringAttribute:ax::mojom::StringAttribute::kPlaceholder];
}

- (NSString*)accessibilitySelectedText {
  if (!_node)
    return nil;

  return [self AXSelectedText];
}

- (NSRange)accessibilitySelectedTextRange {
  if (!_node)
    return NSMakeRange(0, 0);

  NSRange r;
  [[self AXSelectedTextRange] getValue:&r];
  return r;
}

- (NSArray*)accessibilitySelectedTextRanges {
  if (!_node)
    return nil;

  return @[ [self AXSelectedTextRange] ];
}

- (NSRange)accessibilityVisibleCharacterRange {
  if (!_node)
    return NSMakeRange(0, 0);

  return [[self AXVisibleCharacterRange] rangeValue];
}

- (NSString*)accessibilityStringForRange:(NSRange)range {
  if (!_node)
    return nil;

  return (NSString*)[self AXStringForRange:[NSValue valueWithRange:range]];
}

- (NSAttributedString*)accessibilityAttributedStringForRange:(NSRange)range {
  if (!_node)
    return nil;

  return [self AXAttributedStringForRange:[NSValue valueWithRange:range]];
}

- (NSInteger)accessibilityLineForIndex:(NSInteger)index {
  if (!_node)
    return 0;

  return
      [[self AXLineForIndex:[NSNumber numberWithInteger:index]] integerValue];
}

- (NSRange)accessibilityRangeForIndex:(NSInteger)index {
  if (!_node)
    return NSMakeRange(0, 0);

  return [[self AXRangeForIndex:[NSNumber numberWithInteger:index]] rangeValue];
}

- (NSRange)accessibilityStyleRangeForIndex:(NSInteger)index {
  if (!_node)
    return NSMakeRange(0, 0);

  return [[self AXStyleRangeForIndex:[NSNumber numberWithInteger:index]]
      rangeValue];
}

- (NSRange)accessibilityRangeForLine:(NSInteger)line {
  if (!_node)
    return NSMakeRange(0, 0);

  return [[self AXRangeForLine:[NSNumber numberWithInteger:line]] rangeValue];
}

- (NSRange)accessibilityRangeForPosition:(NSPoint)point {
  return [[self AXRangeForPosition:[NSValue valueWithPoint:point]] rangeValue];
}

//
// NSAccessibility protocol: setting content and values.
//

- (NSURL*)accessibilityURL {
  TRACE_EVENT1("accessibility", "accessibilityURL",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return nil;

  std::string url;
  if ([[self accessibilityRole] isEqualToString:NSAccessibilityWebAreaRole])
    url = _node->GetDelegate()->GetTreeData().url;
  else
    url = _node->GetStringAttribute(ax::mojom::StringAttribute::kUrl);

  if (url.empty())
    return nil;

  return [NSURL URLWithString:(base::SysUTF8ToNSString(url))];
}

//
// NSAccessibility protocol: assigning roles.
//

- (NSString*)accessibilityRoleDescription {
  TRACE_EVENT1("accessibility", "accessibilityRoleDescription",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return nil;

  // Image annotations.
  if (_node->GetData().GetImageAnnotationStatus() ==
          ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation ||
      _node->GetData().GetImageAnnotationStatus() ==
          ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation) {
    return base::SysUTF16ToNSString(
        _node->GetDelegate()->GetLocalizedRoleDescriptionForUnlabeledImage());
  }

  // ARIA role description.
  std::string roleDescription;
  if (_node->GetStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                                &roleDescription)) {
    return [base::SysUTF8ToNSString(_node->GetStringAttribute(
        ax::mojom::StringAttribute::kRoleDescription)) lowercaseString];
  }

  NSString* role = [self accessibilityRole];

  // The following descriptions are specific to webkit.
  if ([role isEqualToString:NSAccessibilityWebAreaRole]) {
    return l10n_util::GetNSString(IDS_AX_ROLE_WEB_AREA);
  }

  if ([role isEqualToString:NSAccessibilityLinkRole]) {
    return l10n_util::GetNSString(IDS_AX_ROLE_LINK);
  }

  if ([role isEqualToString:@"AXHeading"]) {
    return l10n_util::GetNSString(IDS_AX_ROLE_HEADING);
  }

  if (([role isEqualToString:NSAccessibilityGroupRole] ||
       [role isEqualToString:NSAccessibilityRadioButtonRole]) &&
      !_node->GetDelegate()->IsWebAreaForPresentationalIframe()) {
    std::string role_attribute;
    if (_node->GetHtmlAttribute("role", &role_attribute)) {
      ax::mojom::Role internalRole = _node->GetRole();
      if ((internalRole != ax::mojom::Role::kBlockquote &&
           internalRole != ax::mojom::Role::kCaption &&
           internalRole != ax::mojom::Role::kGroup &&
           internalRole != ax::mojom::Role::kListItem &&
           internalRole != ax::mojom::Role::kMark &&
           internalRole != ax::mojom::Role::kParagraph) ||
          internalRole == ax::mojom::Role::kTab) {
        // TODO(dtseng): This is not localized; see crbug/84814.
        return base::SysUTF8ToNSString(role_attribute);
      }
    }
  }

  switch (_node->GetRole()) {
    case ax::mojom::Role::kArticle:
      return l10n_util::GetNSString(IDS_AX_ROLE_ARTICLE);
    case ax::mojom::Role::kBanner:
      return l10n_util::GetNSString(IDS_AX_ROLE_BANNER);
    case ax::mojom::Role::kCheckBox:
      return l10n_util::GetNSString(IDS_AX_ROLE_CHECK_BOX);
    case ax::mojom::Role::kComment:
      return l10n_util::GetNSString(IDS_AX_ROLE_COMMENT);
    case ax::mojom::Role::kComplementary:
      return l10n_util::GetNSString(IDS_AX_ROLE_COMPLEMENTARY);
    case ax::mojom::Role::kContentInfo:
      return l10n_util::GetNSString(IDS_AX_ROLE_CONTENT_INFO);
    case ax::mojom::Role::kDescriptionList:
      return l10n_util::GetNSString(IDS_AX_ROLE_DESCRIPTION_LIST);
    case ax::mojom::Role::kDescriptionListDetail:
      return l10n_util::GetNSString(IDS_AX_ROLE_DEFINITION);
    case ax::mojom::Role::kDescriptionListTerm:
      return l10n_util::GetNSString(IDS_AX_ROLE_DESCRIPTION_TERM);
    case ax::mojom::Role::kDisclosureTriangle:
      return l10n_util::GetNSString(IDS_AX_ROLE_DISCLOSURE_TRIANGLE);
    case ax::mojom::Role::kFigure:
      return l10n_util::GetNSString(IDS_AX_ROLE_FIGURE);
    case ax::mojom::Role::kFooter:
      return l10n_util::GetNSString(IDS_AX_ROLE_FOOTER);
    case ax::mojom::Role::kForm:
      return l10n_util::GetNSString(IDS_AX_ROLE_FORM);
    case ax::mojom::Role::kHeader:
      return l10n_util::GetNSString(IDS_AX_ROLE_BANNER);
    case ax::mojom::Role::kMain:
      return l10n_util::GetNSString(IDS_AX_ROLE_MAIN_CONTENT);
    case ax::mojom::Role::kMark:
      return l10n_util::GetNSString(IDS_AX_ROLE_MARK);
    case ax::mojom::Role::kMath:
    case ax::mojom::Role::kMathMLMath:
      return l10n_util::GetNSString(IDS_AX_ROLE_MATH);
    case ax::mojom::Role::kNavigation:
      return l10n_util::GetNSString(IDS_AX_ROLE_NAVIGATIONAL_LINK);
    case ax::mojom::Role::kRegion:
      return l10n_util::GetNSString(IDS_AX_ROLE_REGION);
    case ax::mojom::Role::kSpinButton:
      // This control is similar to what VoiceOver calls a "stepper".
      return l10n_util::GetNSString(IDS_AX_ROLE_STEPPER);
    case ax::mojom::Role::kStatus:
      return l10n_util::GetNSString(IDS_AX_ROLE_STATUS);
    case ax::mojom::Role::kSearchBox:
      return l10n_util::GetNSString(IDS_AX_ROLE_SEARCH_BOX);
    case ax::mojom::Role::kSuggestion:
      return l10n_util::GetNSString(IDS_AX_ROLE_SUGGESTION);
    case ax::mojom::Role::kSwitch:
      return l10n_util::GetNSString(IDS_AX_ROLE_SWITCH);
    case ax::mojom::Role::kTab:
      // There is no NSAccessibilityTabRole or similar (AXRadioButton is used
      // instead). Do the same as NSTabView and put "tab" in the description.
      return l10n_util::GetNSString(IDS_AX_ROLE_TAB);
    case ax::mojom::Role::kTerm:
      return l10n_util::GetNSString(IDS_AX_ROLE_DESCRIPTION_TERM);
    case ax::mojom::Role::kToggleButton:
      return l10n_util::GetNSString(IDS_AX_ROLE_TOGGLE_BUTTON);
    default:
      break;
  }

  return NSAccessibilityRoleDescription(role, [self accessibilitySubrole]);
}

//
// NSAccessibility protocol: configuring table and outline views.
//

- (NSArray*)accessibilityColumnHeaderUIElements {
  if (![self instanceActive])
    return nil;

  ui::AXPlatformNodeDelegate* delegate = _node->GetDelegate();
  DCHECK(delegate);

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];

  // If this is a table, return all column headers.
  ax::mojom::Role role = _node->GetRole();
  if (ui::IsTableLike(role)) {
    for (ui::AXNodeID id : delegate->GetColHeaderNodeIds()) {
      AXPlatformNodeCocoa* colheader = [self fromNodeID:id];
      if (colheader)
        [ret addObject:colheader];
    }
    return [ret count] ? ret : nil;
  }

  // Otherwise if this is a cell or a header cell, return the column headers for
  // it.
  if (!ui::IsCellOrTableHeader(role))
    return nil;

  ui::AXPlatformNodeBase* table = _node->GetTable();
  if (!table)
    return nil;

  absl::optional<int> column = delegate->GetTableCellColIndex();
  if (!column)
    return nil;

  ui::AXPlatformNodeDelegate* tableDelegate = table->GetDelegate();
  DCHECK(tableDelegate);
  for (ui::AXNodeID id : tableDelegate->GetColHeaderNodeIds(*column)) {
    AXPlatformNodeCocoa* colheader = [self fromNodeID:id];
    if (colheader)
      [ret addObject:colheader];
  }
  return [ret count] ? ret : nil;
}

// MathML attributes.
// TODO(crbug.com/1051115): The MathML aam considers only in-flow children.
// TODO(crbug.com/1051115): When/if it is needed to expose this for other a11y
// APIs, then some of the logic below should probably be moved to the
// platform-independent classes.

- (id)AXMathFractionNumerator {
  if (![self instanceActive] ||
      _node->GetRole() != ax::mojom::Role::kMathMLFraction) {
    return nil;
  }
  NSArray* children = [self AXChildren];
  if ([children count] >= 1)
    return children[0];
  return nil;
}

- (id)AXMathFractionDenominator {
  if (![self instanceActive] ||
      _node->GetRole() != ax::mojom::Role::kMathMLFraction) {
    return nil;
  }
  NSArray* children = [self AXChildren];
  if ([children count] >= 2)
    return children[1];
  return nil;
}

- (id)AXMathRootRadicand {
  if (![self instanceActive] ||
      !(_node->GetRole() == ax::mojom::Role::kMathMLRoot ||
        _node->GetRole() == ax::mojom::Role::kMathMLSquareRoot)) {
    return nil;
  }
  NSArray* children = [self AXChildren];
  if (_node->GetRole() == ax::mojom::Role::kMathMLRoot) {
    if ([children count] >= 1)
      return [NSArray arrayWithObjects:children[0], nil];
    return nil;
  }
  return children;
}

- (id)AXMathRootIndex {
  if (![self instanceActive] ||
      _node->GetRole() != ax::mojom::Role::kMathMLRoot) {
    return nil;
  }
  NSArray* children = [self AXChildren];
  if ([children count] >= 2)
    return children[1];
  return nil;
}

- (id)AXMathBase {
  if (![self instanceActive] ||
      !(_node->GetRole() == ax::mojom::Role::kMathMLSub ||
        _node->GetRole() == ax::mojom::Role::kMathMLSup ||
        _node->GetRole() == ax::mojom::Role::kMathMLSubSup ||
        _node->GetRole() == ax::mojom::Role::kMathMLUnder ||
        _node->GetRole() == ax::mojom::Role::kMathMLOver ||
        _node->GetRole() == ax::mojom::Role::kMathMLUnderOver ||
        _node->GetRole() == ax::mojom::Role::kMathMLMultiscripts)) {
    return nil;
  }
  NSArray* children = [self AXChildren];
  if ([children count] >= 1)
    return children[0];
  return nil;
}

- (id)AXMathUnder {
  if (![self instanceActive] ||
      !(_node->GetRole() == ax::mojom::Role::kMathMLUnder ||
        _node->GetRole() == ax::mojom::Role::kMathMLUnderOver)) {
    return nil;
  }
  NSArray* children = [self AXChildren];
  if ([children count] >= 2)
    return children[1];
  return nil;
}

- (id)AXMathOver {
  if (![self instanceActive] ||
      !(_node->GetRole() == ax::mojom::Role::kMathMLOver ||
        _node->GetRole() == ax::mojom::Role::kMathMLUnderOver)) {
    return nil;
  }
  NSArray* children = [self AXChildren];
  if (_node->GetRole() == ax::mojom::Role::kMathMLOver &&
      [children count] >= 2) {
    return children[1];
  }
  if (_node->GetRole() == ax::mojom::Role::kMathMLUnderOver &&
      [children count] >= 3) {
    return children[2];
  }
  return nil;
}

- (id)AXMathSubscript {
  if (![self instanceActive] ||
      !(_node->GetRole() == ax::mojom::Role::kMathMLSub ||
        _node->GetRole() == ax::mojom::Role::kMathMLSubSup)) {
    return nil;
  }
  NSArray* children = [self AXChildren];
  if ([children count] >= 2)
    return children[1];
  return nil;
}

- (id)AXMathSuperscript {
  if (![self instanceActive] ||
      !(_node->GetRole() == ax::mojom::Role::kMathMLSup ||
        _node->GetRole() == ax::mojom::Role::kMathMLSubSup)) {
    return nil;
  }
  NSArray* children = [self AXChildren];
  if (_node->GetRole() == ax::mojom::Role::kMathMLSup &&
      [children count] >= 2) {
    return children[1];
  }
  if (_node->GetRole() == ax::mojom::Role::kMathMLSubSup &&
      [children count] >= 3) {
    return children[2];
  }
  return nil;
}

static NSDictionary* createMathSubSupScriptsPair(
    AXPlatformNodeCocoa* subscript,
    AXPlatformNodeCocoa* superscript) {
  AXPlatformNodeCocoa* nodes[2];
  NSString* keys[2];
  NSUInteger count = 0;
  if (subscript) {
    nodes[count] = subscript;
    keys[count] = NSAccessibilityMathSubscriptAttribute;
    count++;
  }
  if (superscript) {
    nodes[count] = superscript;
    keys[count] = NSAccessibilityMathSuperscriptAttribute;
    count++;
  }
  return [[NSDictionary alloc] initWithObjects:nodes forKeys:keys count:count];
}

- (NSArray*)AXMathPostscripts {
  if (![self instanceActive] ||
      _node->GetRole() != ax::mojom::Role::kMathMLMultiscripts)
    return nil;
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  bool foundBaseElement = false;
  AXPlatformNodeCocoa* subscript = nullptr;
  for (AXPlatformNodeCocoa* child in [self AXChildren]) {
    if ([child internalRole] == ax::mojom::Role::kMathMLPrescriptDelimiter)
      break;
    if (!foundBaseElement) {
      foundBaseElement = true;
      continue;
    }
    if (!subscript) {
      subscript = child;
      continue;
    }
    AXPlatformNodeCocoa* superscript = child;
    [ret addObject:createMathSubSupScriptsPair(subscript, superscript)];
    subscript = nullptr;
  }
  return [ret count] ? ret : nil;
}

- (NSArray*)AXMathPrescripts {
  if (![self instanceActive] ||
      _node->GetRole() != ax::mojom::Role::kMathMLMultiscripts)
    return nil;
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  bool foundPrescriptDelimiter = false;
  AXPlatformNodeCocoa* subscript = nullptr;
  for (AXPlatformNodeCocoa* child in [self AXChildren]) {
    if (!foundPrescriptDelimiter) {
      foundPrescriptDelimiter =
          ([child internalRole] == ax::mojom::Role::kMathMLPrescriptDelimiter);
      continue;
    }
    if (!subscript) {
      subscript = child;
      continue;
    }
    AXPlatformNodeCocoa* superscript = child;
    [ret addObject:createMathSubSupScriptsPair(subscript, superscript)];
    subscript = nullptr;
  }
  return [ret count] ? ret : nil;
}

@end
