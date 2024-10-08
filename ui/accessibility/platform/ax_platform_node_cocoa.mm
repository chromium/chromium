// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/accessibility/platform/ax_platform_node_cocoa.h"

#import <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_platform_node_mac.h"
#include "ui/accessibility/platform/ax_private_attributes_mac.h"
#include "ui/accessibility/platform/ax_private_roles_mac.h"
#include "ui/accessibility/platform/ax_utils_mac.h"
#include "ui/accessibility/platform/child_iterator.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/strings/grit/ax_strings.h"

using AXRange = ui::AXPlatformNodeDelegate::AXRange;

@interface AXAnnouncementSpec ()

@property(nonatomic, strong) NSString* announcement;
@property(nonatomic, strong) NSWindow* window;
@property(nonatomic, assign) BOOL polite;

@end

@implementation AXAnnouncementSpec

@synthesize announcement = _announcement;
@synthesize window = _window;
@synthesize polite = _polite;

@end

namespace {

// Same length as web content/WebKit.
int kLiveRegionDebounceMillis = 20;

using RoleMap = std::map<ax::mojom::Role, NSString*>;
using EventMap = std::map<ax::mojom::Event, NSString*>;
using ActionList = std::vector<std::pair<ax::mojom::Action, NSString*>>;

RoleMap BuildSubroleMap() {
  const RoleMap::value_type subroles[] = {
      {ax::mojom::Role::kAlert, @"AXApplicationAlert"},
      {ax::mojom::Role::kAlertDialog, @"AXApplicationAlertDialog"},
      {ax::mojom::Role::kApplication, @"AXWebApplication"},
      {ax::mojom::Role::kArticle, @"AXDocumentArticle"},
      {ax::mojom::Role::kBanner, @"AXLandmarkBanner"},
      {ax::mojom::Role::kCode, @"AXCodeStyleGroup"},
      {ax::mojom::Role::kComplementary, @"AXLandmarkComplementary"},
      {ax::mojom::Role::kContentDeletion, @"AXDeleteStyleGroup"},
      {ax::mojom::Role::kContentInsertion, @"AXInsertStyleGroup"},
      {ax::mojom::Role::kContentInfo, @"AXLandmarkContentInfo"},
      {ax::mojom::Role::kDefinition, @"AXDefinition"},
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
      {ax::mojom::Role::kMeter, @"AXMeter"},
      {ax::mojom::Role::kNavigation, @"AXLandmarkNavigation"},
      {ax::mojom::Role::kNote, @"AXDocumentNote"},
      {ax::mojom::Role::kRegion, @"AXLandmarkRegion"},
      {ax::mojom::Role::kSearch, @"AXLandmarkSearch"},
      {ax::mojom::Role::kSearchBox, @"AXSearchField"},
      {ax::mojom::Role::kSectionFooter, @"AXSectionFooter"},
      {ax::mojom::Role::kSectionHeader, @"AXSectionHeader"},
      {ax::mojom::Role::kStatus, @"AXApplicationStatus"},
      {ax::mojom::Role::kStrong, @"AXStrongStyleGroup"},
      {ax::mojom::Role::kSubscript, @"AXSubscriptStyleGroup"},
      {ax::mojom::Role::kSuperscript, @"AXSuperscriptStyleGroup"},
      {ax::mojom::Role::kSwitch, @"AXSwitch"},
      {ax::mojom::Role::kTab, @"AXTabButton"},
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
         (node.GetRole() == ax::mojom::Role::kPopUpButton ||
          node.GetRole() == ax::mojom::Role::kComboBoxSelect);
}

// Check whether |selector| is an accessibility setter. This is a heuristic but
// seems to be a pretty good one.
bool IsAXSetter(SEL selector) {
  return [NSStringFromSelector(selector) hasPrefix:@"setAccessibility"];
}

void CollectAncestorRoles(
    const ui::AXNode& node,
    std::map<ui::AXNodeID, std::set<ax::mojom::Role>>& out_ancestor_roles) {
  if (out_ancestor_roles.contains(node.id()))
    return;
  out_ancestor_roles[node.id()] = {node.GetRole()};
  if (!node.GetParent())
    return;
  CollectAncestorRoles(*node.GetParent(), out_ancestor_roles);
  out_ancestor_roles[node.id()].insert(
      out_ancestor_roles[node.GetParent()->id()].begin(),
      out_ancestor_roles[node.GetParent()->id()].end());
}

}  // namespace

@interface AXPlatformNodeCocoa (Private)
// Helper function for string attributes that don't require extra processing.
- (NSString*)getStringAttribute:(ax::mojom::StringAttribute)attribute;

// Returns AXValue, or nil if AXValue isn't an NSString.
- (NSString*)getAXValueAsString;

// Returns the native wrapper for the given node id.
- (AXPlatformNodeCocoa*)fromNodeID:(ui::AXNodeID)id;

// Returns true if this object is an image.
- (BOOL)isImage;

@end

@implementation AXPlatformNodeCocoa {
  // This field is not a raw_ptr<> because it requires @property rewrite.
  RAW_PTR_EXCLUSION ui::AXPlatformNodeBase* _node;  // Weak. Retains us.
  AXAnnouncementSpec* __strong _pendingAnnouncement;
}

@synthesize node = _node;
// Required for AXCustomContentProvider, which defines the property.
@synthesize accessibilityCustomContent = _accessibilityCustomContent;

- (NSSet<NSString*>*)migratingAttributes {
  static NSSet<NSString*>* set = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    set = [NSSet<NSString*>
        setWithObjects:NSAccessibilityDisclosedByRowAttribute,
                       NSAccessibilityDisclosedRowsAttribute,
                       NSAccessibilityDisclosingAttribute,
                       NSAccessibilityDisclosureLevelAttribute,
                       NSAccessibilityFocusedAttribute, nil];
  });
  return set;
}

- (NSSet<NSString*>*)migratingMethods {
  static NSSet<NSString*>* set = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    set = [NSSet<NSString*> setWithObjects:@"accessibilityDisclosedByRow",
                                           @"accessibilityDisclosedRows",
                                           @"accessibilityDisclosureLevel",
                                           @"isAccessibilityDisclosed",
                                           @"isAccessibilityFocused", nil];
  });
  return set;
}

- (BOOL)isMigratingAttribute:(NSString*)attribute {
  if (features::IsMacAccessibilityAPIMigrationEnabled()) {
    return [[self migratingAttributes] containsObject:attribute];
    ;
  }
  return NO;
}

- (BOOL)respondsToSelector:(SEL)selector {
  if (!features::IsMacAccessibilityAPIMigrationEnabled()) {
    if ([[self migratingMethods]
            containsObject:NSStringFromSelector(selector)]) {
      return NO;
    }
  }
  return [super respondsToSelector:selector];
}

- (ui::AXPlatformNodeDelegate*)nodeDelegate {
  return _node ? _node->GetDelegate() : nil;
}

- (BOOL)instanceActive {
  return _node != nullptr;
}

- (BOOL)isIncludedInPlatformTree {
  // TODO(accessibility): Do we really need to have invisible objects in
  // the platform tree?
  return [self instanceActive] &&
         ![[self AXRole] isEqualToString:NSAccessibilityUnknownRole] &&
         !_node->IsInvisibleOrIgnored();
}

- (id)titleUIElement {
  // True only if it's a control, if there's a single label, and the label has
  // nonempty text.

  // VoiceOver ignores TitleUIElement if the element isn't a control.
  if (!ui::IsControl(_node->GetRole()))
    return nil;

  if (!_node->HasNameFromOtherElement())
    return nil;

  std::vector<int32_t> labelledby_ids =
      _node->GetIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds);
  if (labelledby_ids.size() != 1)
    return nil;

  ui::AXPlatformNode* label =
      _node->GetDelegate()->GetFromNodeID(labelledby_ids[0]);
  if (!label)
    return nil;

  // No title UI element if the label's name is empty.
  std::string labelName = label->GetDelegate()->GetName();
  if (labelName.empty())
    return nil;

  // In the case where we have a radio button or a checked box, no title UI
  // element. This goes against Apple's documentation for AXTitleUIElement,
  // but is consistent with Safari+Voiceover behavior.
  // See crbug.com/1430419
  ax::mojom::Role role = _node->GetRole();
  if (ui::IsRadio(role) || ui::IsCheckBox(role))
    return nil;

  return label->GetNativeViewAccessible();
}

- (BOOL)isNameFromLabel {
  // Image annotations are not visible text, so they should be exposed
  // as a description and not a title.
  switch (_node->GetData().GetImageAnnotationStatus()) {
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      return true;

    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
      break;
  }

  // No label for windows or native dialogs.
  ax::mojom::Role role = _node->GetRole();
  if (ui::IsWindow(role) ||
      (ui::IsDialog(role) && !_node->GetDelegate()->IsWebContent()))
    return false;

  // VoiceOver computes the wrong description for a link.
  if (ui::IsLink(role))
    return true;

  // If a radiobutton or checkbox has a single label, we are consistent
  // with Safari+Voiceover and expose it via AccessibilityLabel.
  // Note: Safari+Voiceover is inconsistent with Apple's documentation,
  // which suggests this should be exposed via AXTitleUIElement. See
  // crbug.com/1430419
  if (ui::IsRadio(role) || ui::IsCheckBox(role)) {
    std::vector<int32_t> labelledby_ids =
        _node->GetIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds);
    if (labelledby_ids.size() == 1) {
      ui::AXPlatformNode* label =
          _node->GetDelegate()->GetFromNodeID(labelledby_ids[0]);
      if (label) {
        // No title UI element if the label's name is empty.
        std::string labelName = label->GetDelegate()->GetName();
        if (!labelName.empty())
          return true;
      }
    }
  }

  // VoiceOver will not read the label of these roles unless it is
  // exposed in the description instead of the title.
  switch (role) {
    case ax::mojom::Role::kGenericContainer:
    case ax::mojom::Role::kGroup:
    case ax::mojom::Role::kRadioGroup:
    case ax::mojom::Role::kTabPanel:
      return true;
    default:
      break;
  }

  // On macOS, the accessible name of an object is exposed as its title if it
  // comes from visible text, and as its description otherwise, but never both.
  //
  // Note: a placeholder is often visible text, but since it aids in data entry
  // it is similar to accessibilityValue, and thus cannot be exposed either in
  // accessibilityTitle or in accessibilityLabel.
  ax::mojom::NameFrom nameFrom = _node->GetNameFrom();
  if (nameFrom == ax::mojom::NameFrom::kCaption ||
      nameFrom == ax::mojom::NameFrom::kContents ||
      nameFrom == ax::mojom::NameFrom::kPlaceholder ||
      nameFrom == ax::mojom::NameFrom::kRelatedElement ||
      nameFrom == ax::mojom::NameFrom::kValue) {
    return false;
  }

  return true;
}

+ (NSString*)nativeRoleFromAXRole:(ax::mojom::Role)role {
  switch (role) {
    case ax::mojom::Role::kAbbr:
    case ax::mojom::Role::kAlert:
    case ax::mojom::Role::kAlertDialog:
    case ax::mojom::Role::kApplication:
    case ax::mojom::Role::kArticle:
    case ax::mojom::Role::kAudio:
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kBlockquote:
    case ax::mojom::Role::kCaption:
    case ax::mojom::Role::kClient:
    case ax::mojom::Role::kCode:
    case ax::mojom::Role::kComment:
    case ax::mojom::Role::kComplementary:
    case ax::mojom::Role::kContentDeletion:
    case ax::mojom::Role::kContentInsertion:
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kDefinition:
    case ax::mojom::Role::kDesktop:
    case ax::mojom::Role::kDialog:
    case ax::mojom::Role::kDetails:
    case ax::mojom::Role::kDocAbstract:
    case ax::mojom::Role::kDocAcknowledgments:
    case ax::mojom::Role::kDocAfterword:
    case ax::mojom::Role::kDocAppendix:
    case ax::mojom::Role::kDocBiblioEntry:
    case ax::mojom::Role::kDocBibliography:
    case ax::mojom::Role::kDocChapter:
    case ax::mojom::Role::kDocColophon:
    case ax::mojom::Role::kDocConclusion:
    case ax::mojom::Role::kDocCredit:
    case ax::mojom::Role::kDocCredits:
    case ax::mojom::Role::kDocDedication:
    case ax::mojom::Role::kDocEndnote:
    case ax::mojom::Role::kDocEndnotes:
    case ax::mojom::Role::kDocEpigraph:
    case ax::mojom::Role::kDocEpilogue:
    case ax::mojom::Role::kDocErrata:
    case ax::mojom::Role::kDocExample:
    case ax::mojom::Role::kDocFootnote:
    case ax::mojom::Role::kDocForeword:
    case ax::mojom::Role::kDocGlossary:
    case ax::mojom::Role::kDocIndex:
    case ax::mojom::Role::kDocIntroduction:
    case ax::mojom::Role::kDocNotice:
    case ax::mojom::Role::kDocPageFooter:
    case ax::mojom::Role::kDocPageHeader:
    case ax::mojom::Role::kDocPageList:
    case ax::mojom::Role::kDocPart:
    case ax::mojom::Role::kDocPreface:
    case ax::mojom::Role::kDocPrologue:
    case ax::mojom::Role::kDocPullquote:
    case ax::mojom::Role::kDocQna:
    case ax::mojom::Role::kDocTip:
    case ax::mojom::Role::kDocToc:
    case ax::mojom::Role::kDocument:
    case ax::mojom::Role::kEmbeddedObject:
    case ax::mojom::Role::kEmphasis:
    case ax::mojom::Role::kFeed:
    case ax::mojom::Role::kFigcaption:
    case ax::mojom::Role::kFigure:
    case ax::mojom::Role::kFooter:
    case ax::mojom::Role::kForm:
    case ax::mojom::Role::kGenericContainer:
    case ax::mojom::Role::kGraphicsDocument:
    case ax::mojom::Role::kGraphicsObject:
    case ax::mojom::Role::kGroup:
    case ax::mojom::Role::kHeader:
    case ax::mojom::Role::kIframe:
    case ax::mojom::Role::kIframePresentational:
    case ax::mojom::Role::kLabelText:
    case ax::mojom::Role::kLayoutTable:
    case ax::mojom::Role::kLayoutTableCell:
    case ax::mojom::Role::kLayoutTableRow:
    case ax::mojom::Role::kLegend:
    case ax::mojom::Role::kLineBreak:
    case ax::mojom::Role::kListItem:
    case ax::mojom::Role::kLog:
    case ax::mojom::Role::kMain:
    case ax::mojom::Role::kMark:
    case ax::mojom::Role::kMarquee:
    case ax::mojom::Role::kMath:
    case ax::mojom::Role::kMathMLFraction:
    case ax::mojom::Role::kMathMLIdentifier:
    case ax::mojom::Role::kMathMLMath:
    case ax::mojom::Role::kMathMLMultiscripts:
    case ax::mojom::Role::kMathMLNoneScript:
    case ax::mojom::Role::kMathMLNumber:
    case ax::mojom::Role::kMathMLOperator:
    case ax::mojom::Role::kMathMLOver:
    case ax::mojom::Role::kMathMLPrescriptDelimiter:
    case ax::mojom::Role::kMathMLRoot:
    case ax::mojom::Role::kMathMLRow:
    case ax::mojom::Role::kMathMLSquareRoot:
    case ax::mojom::Role::kMathMLStringLiteral:
    case ax::mojom::Role::kMathMLSub:
    case ax::mojom::Role::kMathMLSubSup:
    case ax::mojom::Role::kMathMLSup:
    case ax::mojom::Role::kMathMLTable:
    case ax::mojom::Role::kMathMLTableCell:
    case ax::mojom::Role::kMathMLTableRow:
    case ax::mojom::Role::kMathMLText:
    case ax::mojom::Role::kMathMLUnder:
    case ax::mojom::Role::kMathMLUnderOver:
    case ax::mojom::Role::kNavigation:
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kNote:
    case ax::mojom::Role::kPane:
    case ax::mojom::Role::kParagraph:
    case ax::mojom::Role::kPdfRoot:
    case ax::mojom::Role::kPluginObject:
    case ax::mojom::Role::kRegion:
    case ax::mojom::Role::kRowGroup:
    case ax::mojom::Role::kRuby:
    case ax::mojom::Role::kSearch:
    case ax::mojom::Role::kSection:
    case ax::mojom::Role::kSectionFooter:
    case ax::mojom::Role::kSectionHeader:
    case ax::mojom::Role::kSectionWithoutName:
    case ax::mojom::Role::kStatus:
    case ax::mojom::Role::kSubscript:
    case ax::mojom::Role::kSuggestion:
    case ax::mojom::Role::kSuperscript:
      return NSAccessibilityGroupRole;
    case ax::mojom::Role::kSvgRoot:
      return NSAccessibilityImageRole;
    case ax::mojom::Role::kStrong:
    case ax::mojom::Role::kTableHeaderContainer:
    case ax::mojom::Role::kTabPanel:
    case ax::mojom::Role::kTerm:
    case ax::mojom::Role::kTime:
    case ax::mojom::Role::kTimer:
    case ax::mojom::Role::kTooltip:
    case ax::mojom::Role::kVideo:
    case ax::mojom::Role::kWebView:
      return NSAccessibilityGroupRole;
    case ax::mojom::Role::kButton:
      return NSAccessibilityButtonRole;
    case ax::mojom::Role::kCanvas:
      return NSAccessibilityImageRole;
    case ax::mojom::Role::kCaret:
      return NSAccessibilityUnknownRole;
    case ax::mojom::Role::kCell:
      return @"AXCell";
    case ax::mojom::Role::kCheckBox:
      return NSAccessibilityCheckBoxRole;
    case ax::mojom::Role::kColorWell:
      return NSAccessibilityColorWellRole;
    case ax::mojom::Role::kColumn:
      return NSAccessibilityColumnRole;
    case ax::mojom::Role::kColumnHeader:
      return @"AXCell";
    case ax::mojom::Role::kComboBoxGrouping:
      return NSAccessibilityComboBoxRole;
    case ax::mojom::Role::kComboBoxMenuButton:
      return NSAccessibilityComboBoxRole;
    case ax::mojom::Role::kComboBoxSelect:
      // TODO(crbug.com/40864556): Can this be NSAccessibilityComboBoxRole?
      return NSAccessibilityPopUpButtonRole;
    case ax::mojom::Role::kDate:
      return @"AXDateField";
    case ax::mojom::Role::kDateTime:
      return @"AXDateField";
    case ax::mojom::Role::kDescriptionList:
      return NSAccessibilityListRole;
    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kDisclosureTriangleGrouped:
      // If Mac supports AXExpandedChanged event with
      // NSAccessibilityDisclosureTriangleRole, We should update
      // ax::mojom::Role::kDisclosureTriangle mapping to
      // NSAccessibilityDisclosureTriangleRole. http://crbug.com/558324
      return features::IsAccessibilityExposeSummaryAsHeadingEnabled()
                 ? NSAccessibilityDisclosureTriangleRole
                 : NSAccessibilityButtonRole;
    case ax::mojom::Role::kDocBackLink:
    case ax::mojom::Role::kDocBiblioRef:
    case ax::mojom::Role::kDocGlossRef:
    case ax::mojom::Role::kDocNoteRef:
      return NSAccessibilityLinkRole;
    case ax::mojom::Role::kDocCover:
      return NSAccessibilityImageRole;
    case ax::mojom::Role::kDocPageBreak:
      return NSAccessibilitySplitterRole;
    case ax::mojom::Role::kDocSubtitle:
      return @"AXHeading";
    case ax::mojom::Role::kGraphicsSymbol:
      return NSAccessibilityImageRole;
    case ax::mojom::Role::kGrid:
      // Should be NSAccessibilityGridRole but VoiceOver treating it like
      // a list as of 10.12.6, so following WebKit and using table role:
      // crbug.com/753925
      return NSAccessibilityTableRole;
    case ax::mojom::Role::kGridCell:
      return @"AXCell";
    case ax::mojom::Role::kHeading:
      return @"AXHeading";
    case ax::mojom::Role::kImage:
      return NSAccessibilityImageRole;
    case ax::mojom::Role::kImeCandidate:
      return NSAccessibilityUnknownRole;
    case ax::mojom::Role::kInlineTextBox:
      return NSAccessibilityStaticTextRole;
    case ax::mojom::Role::kInputTime:
      return @"AXTimeField";
    case ax::mojom::Role::kKeyboard:
      return NSAccessibilityUnknownRole;
    case ax::mojom::Role::kLink:
      return NSAccessibilityLinkRole;
    case ax::mojom::Role::kList:
      return NSAccessibilityListRole;
    case ax::mojom::Role::kListBox:
      return NSAccessibilityListRole;
    case ax::mojom::Role::kListBoxOption:
      return NSAccessibilityStaticTextRole;
    case ax::mojom::Role::kListGrid:
      return NSAccessibilityTableRole;
    case ax::mojom::Role::kListMarker:
      return @"AXListMarker";
    case ax::mojom::Role::kMenu:
      return NSAccessibilityMenuRole;
    case ax::mojom::Role::kMenuBar:
      return NSAccessibilityMenuBarRole;
    case ax::mojom::Role::kMenuItem:
      return NSAccessibilityMenuItemRole;
    case ax::mojom::Role::kMenuItemCheckBox:
      return NSAccessibilityMenuItemRole;
    case ax::mojom::Role::kMenuItemRadio:
      return NSAccessibilityMenuItemRole;
    case ax::mojom::Role::kMenuListOption:
      return NSAccessibilityMenuItemRole;
    case ax::mojom::Role::kMenuListPopup:
      return NSAccessibilityMenuRole;
    case ax::mojom::Role::kMeter:
      return NSAccessibilityLevelIndicatorRole;
    case ax::mojom::Role::kPdfActionableHighlight:
      return NSAccessibilityButtonRole;
    case ax::mojom::Role::kPopUpButton:
      return NSAccessibilityPopUpButtonRole;
    case ax::mojom::Role::kProgressIndicator:
      return NSAccessibilityProgressIndicatorRole;
    case ax::mojom::Role::kRadioButton:
      return NSAccessibilityRadioButtonRole;
    case ax::mojom::Role::kRadioGroup:
      return NSAccessibilityRadioGroupRole;
    case ax::mojom::Role::kRootWebArea:
      return NSAccessibilityWebAreaRole;
    case ax::mojom::Role::kRow:
      return NSAccessibilityRowRole;
    case ax::mojom::Role::kRowHeader:
      return @"AXCell";
    case ax::mojom::Role::kRubyAnnotation:
      return NSAccessibilityUnknownRole;
    case ax::mojom::Role::kScrollBar:
      return NSAccessibilityScrollBarRole;
    case ax::mojom::Role::kScrollView:
      return NSAccessibilityScrollAreaRole;
    case ax::mojom::Role::kSearchBox:
      return NSAccessibilityTextFieldRole;
    case ax::mojom::Role::kSlider:
      return NSAccessibilitySliderRole;
    case ax::mojom::Role::kSpinButton:
      return NSAccessibilityIncrementorRole;
    case ax::mojom::Role::kSplitter:
      return NSAccessibilitySplitterRole;
    case ax::mojom::Role::kStaticText:
      return NSAccessibilityStaticTextRole;
    case ax::mojom::Role::kSwitch:
      return NSAccessibilityCheckBoxRole;
    case ax::mojom::Role::kTab:
      return NSAccessibilityRadioButtonRole;
    case ax::mojom::Role::kTable:
      return NSAccessibilityTableRole;
    case ax::mojom::Role::kTabList:
      return NSAccessibilityTabGroupRole;
    case ax::mojom::Role::kTextField:
      return NSAccessibilityTextFieldRole;
    case ax::mojom::Role::kTextFieldWithComboBox:
      return NSAccessibilityComboBoxRole;
    case ax::mojom::Role::kTitleBar:
      return NSAccessibilityStaticTextRole;
    case ax::mojom::Role::kToggleButton:
      return NSAccessibilityCheckBoxRole;
    case ax::mojom::Role::kToolbar:
      return NSAccessibilityToolbarRole;
    case ax::mojom::Role::kTree:
      return NSAccessibilityOutlineRole;
    case ax::mojom::Role::kTreeGrid:
      return NSAccessibilityTableRole;
    case ax::mojom::Role::kTreeItem:
      return NSAccessibilityRowRole;
    case ax::mojom::Role::kUnknown:
      return NSAccessibilityUnknownRole;
    case ax::mojom::Role::kWindow:
      // Use the group role as the BrowserNativeWidgetWindow already provides
      // a kWindow role, and having extra window roles, which are treated
      // specially by screen readers, can break their ability to find the
      // content window. See http://crbug.com/875843 for more information.
      return NSAccessibilityGroupRole;
    case ax::mojom::Role::kDescriptionListTermDeprecated:
    case ax::mojom::Role::kDescriptionListDetailDeprecated:
    case ax::mojom::Role::kDirectoryDeprecated:
    case ax::mojom::Role::kPreDeprecated:
    case ax::mojom::Role::kPortalDeprecated:
      NOTREACHED();
  }
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
  if ([self instanceActive]) {
    ax::mojom::Role role = static_cast<ax::mojom::Role>(_node->GetRole());
    // Make sure to use Role::kPopupButton instead of Role::kButton for all
    // values of kHasPopup. This is normally already true, but the default
    // implementation does not use kPopupButton if aria-haspopup="dialog".
    if (role == ax::mojom::Role::kButton &&
        _node->HasIntAttribute(ax::mojom::IntAttribute::kHasPopup)) {
      return ax::mojom::Role::kPopUpButton;
    }
    return role;
  }

  return ax::mojom::Role::kUnknown;
}

- (AXPlatformNodeCocoa*)fromNodeID:(ui::AXNodeID)id {
  ui::AXPlatformNode* cell = _node->GetDelegate()->GetFromNodeID(id);
  if (cell)
    return cell->GetNativeViewAccessible();
  return nil;
}

- (BOOL)isImage {
  bool has_image_semantics =
      ui::IsImage(_node->GetRole()) &&
      !_node->GetBoolAttribute(ax::mojom::BoolAttribute::kCanvasHasFallback) &&
      !_node->GetChildCount() &&
      _node->GetNameFrom() != ax::mojom::NameFrom::kAttributeExplicitlyEmpty;
#if DCHECK_IS_ON()
  bool is_native_image =
      [[self accessibilityRole] isEqualToString:NSAccessibilityImageRole];
  DCHECK_EQ(is_native_image, has_image_semantics)
      << "\nPresence/lack of native image role do not match the expected "
         "internal semantics:"
      << "\n* Chrome role: " << ui::ToString(_node->GetRole())
      << "\n* NSAccessibility role: " << [self accessibilityRole]
      << "\n* AXNode: " << *_node;
#endif
  return has_image_semantics;
}

- (void)addTextAnnotationsIn:(const AXRange*)axRange
                          to:(NSMutableAttributedString*)attributedString {
  int anchorStartOffset = 0;
  std::map<ui::AXNodeID, std::set<ax::mojom::Role>> ancestor_roles;

  [attributedString beginEditing];
  for (const AXRange& leafTextRange : *axRange) {
    DCHECK(!leafTextRange.IsNull());
    DCHECK_EQ(leafTextRange.anchor()->GetAnchor(),
              leafTextRange.focus()->GetAnchor())
        << "An anchor range should only span a single object.";

    ui::AXNode* anchor = leafTextRange.focus()->GetAnchor();
    DCHECK(anchor) << "A non-null position should have a non-null anchor node.";

    // Add misspelling information
    const std::vector<int32_t>& markerTypes =
        anchor->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes);
    const std::vector<int>& markerStarts =
        anchor->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts);
    const std::vector<int>& markerEnds =
        anchor->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds);

    DCHECK_EQ(markerTypes.size(), markerStarts.size());
    DCHECK_EQ(markerTypes.size(), markerEnds.size());

    for (size_t i = 0; i < markerTypes.size(); ++i) {
      if (!(markerTypes[i] &
            static_cast<int32_t>(ax::mojom::MarkerType::kSpelling))) {
        continue;
      }

      int misspellingStart = anchorStartOffset + markerStarts[i];
      int misspellingEnd = anchorStartOffset + markerEnds[i];
      int misspellingLength = misspellingEnd - misspellingStart;
      DCHECK_LE(static_cast<unsigned long>(misspellingEnd),
                [attributedString length]);
      DCHECK_GT(misspellingLength, 0);
      [attributedString
          addAttribute:NSAccessibilityMarkedMisspelledTextAttribute
                 value:@YES
                 range:NSMakeRange(misspellingStart, misspellingLength)];
    }

    // Add annotation information
    int leafTextLength = leafTextRange.GetText().length();
    DCHECK_LE(static_cast<unsigned long>(anchorStartOffset + leafTextLength),
              attributedString.length);
    NSRange leafRange = NSMakeRange(anchorStartOffset, leafTextLength);

    CollectAncestorRoles(*anchor, ancestor_roles);

    if (ancestor_roles[anchor->id()].contains(ax::mojom::Role::kMark)) {
      [attributedString addAttribute:@"AXHighlight" value:@YES range:leafRange];
    }
    if (ancestor_roles[anchor->id()].contains(ax::mojom::Role::kSuggestion)) {
      [attributedString addAttribute:@"AXIsSuggestion"
                               value:@YES
                               range:leafRange];
    }
    if (ancestor_roles[anchor->id()].contains(
            ax::mojom::Role::kContentDeletion)) {
      [attributedString addAttribute:@"AXIsSuggestedDeletion"
                               value:@YES
                               range:leafRange];
    }
    if (ancestor_roles[anchor->id()].contains(
            ax::mojom::Role::kContentInsertion)) {
      [attributedString addAttribute:@"AXIsSuggestedInsertion"
                               value:@YES
                               range:leafRange];
    }

    ui::AXTextAttributes text_attrs =
        leafTextRange.anchor()->GetTextAttributes();

    NSMutableDictionary* fontAttributes = [NSMutableDictionary dictionary];

    // TODO(crbug.com/41456329): Implement NSAccessibilityFontFamilyKey.
    // TODO(crbug.com/41456329): Implement NSAccessibilityFontNameKey.
    // TODO(crbug.com/41456329): Implement NSAccessibilityVisibleNameKey.

    if (text_attrs.font_size != ui::AXTextAttributes::kUnsetValue) {
      fontAttributes[NSAccessibilityFontSizeKey] = @(text_attrs.font_size);
    }

    if (text_attrs.HasTextStyle(ax::mojom::TextStyle::kBold)) {
      fontAttributes[@"AXFontBold"] = @YES;
    }

    if (text_attrs.HasTextStyle(ax::mojom::TextStyle::kItalic)) {
      fontAttributes[@"AXFontItalic"] = @YES;
    }

    [attributedString addAttribute:NSAccessibilityFontTextAttribute
                             value:fontAttributes
                             range:leafRange];

    if (text_attrs.color != ui::AXTextAttributes::kUnsetValue) {
      [attributedString addAttribute:NSAccessibilityForegroundColorTextAttribute
                               value:(__bridge id)skia::SkColorToSRGBNSColor(
                                         SkColor(text_attrs.color))
                                         .CGColor
                               range:leafRange];
    } else {
      [attributedString
          removeAttribute:NSAccessibilityForegroundColorTextAttribute
                    range:leafRange];
    }

    if (text_attrs.background_color != ui::AXTextAttributes::kUnsetValue) {
      [attributedString addAttribute:NSAccessibilityBackgroundColorTextAttribute
                               value:(__bridge id)skia::SkColorToSRGBNSColor(
                                         SkColor(text_attrs.background_color))
                                         .CGColor
                               range:leafRange];
    } else {
      [attributedString
          removeAttribute:NSAccessibilityBackgroundColorTextAttribute
                    range:leafRange];
    }

    // TODO(crbug.com/41456329): Implement
    // NSAccessibilitySuperscriptTextAttribute.
    // TODO(crbug.com/41456329): Implement NSAccessibilityShadowTextAttribute.

    if (text_attrs.underline_style != ui::AXTextAttributes::kUnsetValue) {
      [attributedString addAttribute:NSAccessibilityUnderlineTextAttribute
                               value:@YES
                               range:leafRange];
    } else {
      [attributedString removeAttribute:NSAccessibilityUnderlineTextAttribute
                                  range:leafRange];
    }

    // TODO(crbug.com/41456329): Implement
    // NSAccessibilityUnderlineColorTextAttribute.

    if (text_attrs.strikethrough_style != ui::AXTextAttributes::kUnsetValue) {
      [attributedString addAttribute:NSAccessibilityStrikethroughTextAttribute
                               value:@YES
                               range:leafRange];
    } else {
      [attributedString
          removeAttribute:NSAccessibilityStrikethroughTextAttribute
                    range:leafRange];
    }

    // TODO(crbug.com/41456329): Implement
    // NSAccessibilityStrikethroughColorTextAttribute.

    // TODO(crbug.com/41456329): Implement NSAccessibilityLinkTextAttribute.
    // TODO(crbug.com/41456329): Implement
    // NSAccessibilityAutocorrectedTextAttribute.

    anchorStartOffset += leafTextLength;
  }
  [attributedString endEditing];
}

- (BOOL)descriptionIsFromAriaDescription {
  ax::mojom::DescriptionFrom descFrom = static_cast<ax::mojom::DescriptionFrom>(
      _node->GetIntAttribute(ax::mojom::IntAttribute::kDescriptionFrom));
  return descFrom == ax::mojom::DescriptionFrom::kAriaDescription ||
         descFrom == ax::mojom::DescriptionFrom::kRelatedElement;
}

- (NSString*)getName {
  return base::SysUTF8ToNSString(_node->GetName());
}

- (AXAnnouncementSpec*)announcementForEvent:(ax::mojom::Event)eventType {
  // Only alerts and live region changes should be announced.
  DCHECK(eventType == ax::mojom::Event::kAlert ||
         eventType == ax::mojom::Event::kLiveRegionChanged);
  std::string liveStatus =
      _node->GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus);
  // If live status is explicitly set to off, don't announce.
  if (liveStatus == "off") {
    return nil;
  }

  NSString* name = [self getName];
  NSString* announcementText =
      name.length > 0 ? name
                      : base::SysUTF16ToNSString(_node->GetTextContentUTF16());
  if (announcementText.length == 0) {
    return nil;
  }

  const std::string& description =
      _node->GetStringAttribute(ax::mojom::StringAttribute::kDescription);
  if (!description.empty()) {
    // Concatenating name and description, with a newline in between to create a
    // pause to avoid treating the concatenation as a single sentence.
    announcementText =
        [NSString stringWithFormat:@"%@\n%@", announcementText,
                                   base::SysUTF8ToNSString(description)];
  }

  AXAnnouncementSpec* spec = [[AXAnnouncementSpec alloc] init];
  spec.announcement = announcementText;
  spec.window = [self AXWindow];
  spec.polite = liveStatus != "assertive";
  return spec;
}

- (void)scheduleLiveRegionAnnouncement:(AXAnnouncementSpec*)announcement {
  if (_pendingAnnouncement) {
    // An announcement is already in flight, so just reset the contents. This is
    // threadsafe because the dispatch is on the main queue.
    _pendingAnnouncement = announcement;
    return;
  }

  _pendingAnnouncement = announcement;
  dispatch_after(
      kLiveRegionDebounceMillis * NSEC_PER_MSEC, dispatch_get_main_queue(), ^{
        if (!self->_pendingAnnouncement) {
          return;
        }
        PostAnnouncementNotification(self->_pendingAnnouncement.announcement,
                                     self->_pendingAnnouncement.window,
                                     self->_pendingAnnouncement.polite);
        self->_pendingAnnouncement = nil;
      });
}

//
// NSAccessibility legacy informal protocol implementation (deprecated).
// https://developer.apple.com/documentation/appkit/deprecated_symbols/nsaccessibility
//

- (BOOL)accessibilityIsIgnored {
  return ![self isAccessibilityElement];
}

- (id)accessibilityHitTest:(NSPoint)point {
  if (!NSPointInRect(point, self.boundsInScreen)) {
    return nil;
  }

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

  NSMutableArray* axActions = [NSMutableArray array];
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

  return axActions;
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

- (NSMutableArray*)internalAccessibilityAttributeNames {
  if (!_node)
    return [NSMutableArray array];

  // These attributes are required on all accessibility objects.
  NSArray* const kAllRoleAttributes = @[
    NSAccessibilityBlockQuoteLevelAttribute, NSAccessibilityChildrenAttribute,
    NSAccessibilityDOMClassList, NSAccessibilityDOMIdentifierAttribute,
    NSAccessibilityDescriptionAttribute, NSAccessibilityElementBusyAttribute,
    NSAccessibilityParentAttribute, NSAccessibilityPositionAttribute,
    NSAccessibilityRoleAttribute, NSAccessibilitySizeAttribute,
    NSAccessibilitySelectedAttribute, NSAccessibilitySizeAttribute,
    NSAccessibilitySubroleAttribute,
    // Title is required for most elements. Cocoa asks for the value even if it
    // is omitted here, but won't present it to accessibility APIs without this.
    NSAccessibilityTitleAttribute,
    // Attributes which are not required, but are general to all roles.
    NSAccessibilityRoleDescriptionAttribute, NSAccessibilityEnabledAttribute,
    NSAccessibilityFocusedAttribute, NSAccessibilityHelpAttribute,
    NSAccessibilityTopLevelUIElementAttribute, NSAccessibilityVisitedAttribute,
    NSAccessibilityWindowAttribute, NSAccessibilityChromeAXNodeIdAttribute
  ];
  // Attributes required for user-editable controls.
  NSArray* const kValueAttributes = @[ NSAccessibilityValueAttribute ];
  // Attributes required for unprotected textfields and labels.
  NSArray* const kUnprotectedTextAttributes = @[
    NSAccessibilityInsertionPointLineNumberAttribute,
    NSAccessibilityNumberOfCharactersAttribute,
    NSAccessibilitySelectedTextAttribute,
    NSAccessibilitySelectedTextRangeAttribute,
    NSAccessibilityVisibleCharacterRangeAttribute
  ];
  // Required for all text, including protected textfields.
  NSString* const kTextAttributes = NSAccessibilityPlaceholderValueAttribute;

  NSMutableArray* axAttributes =
      [NSMutableArray arrayWithArray:kAllRoleAttributes];
  ax::mojom::Role role = _node->GetRole();
  switch (role) {
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
      [axAttributes addObject:NSAccessibilityOwnsAttribute];
      break;
    case ax::mojom::Role::kStaticText:
      [axAttributes addObject:kTextAttributes];
      if (!_node->HasState(ax::mojom::State::kProtected))
        [axAttributes addObjectsFromArray:kUnprotectedTextAttributes];
      [[fallthrough]];
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
  if (ui::IsMenuItem(role))
    [axAttributes addObject:@"AXMenuItemMarkChar"];
  if (ui::IsItemLike(role))
    [axAttributes addObjectsFromArray:@[ @"AXARIAPosInSet", @"AXARIASetSize" ]];
  if (ui::IsSetLike(role))
    [axAttributes addObject:@"AXARIASetSize"];

  if ([[self accessibilityRole] isEqualToString:NSAccessibilityWebAreaRole]) {
    [axAttributes addObjectsFromArray:@[
      NSAccessibilityLoadedAttribute, NSAccessibilityLoadingProgressAttribute
    ]];
  }

  // Caret navigation and text selection attributes.
  if (!ui::IsPlatformDocument(_node->GetRole())) {
    [axAttributes addObject:NSAccessibilityFocusableAncestorAttribute];

    if (_node->HasState(ax::mojom::State::kEditable)) {
      [axAttributes addObjectsFromArray:@[
        NSAccessibilityEditableAncestorAttribute,
        NSAccessibilityHighestEditableAncestorAttribute
      ]];
    }
  }

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

  // AriaBrailleLabel.
  if (_node->HasStringAttribute(ax::mojom::StringAttribute::kAriaBrailleLabel))
    [axAttributes addObject:NSAccessibilityBrailleLabelAttribute];

  // AriaBrailleRoleDescription.
  if (_node->HasStringAttribute(
          ax::mojom::StringAttribute::kAriaBrailleRoleDescription))
    [axAttributes addObject:NSAccessibilityBrailleRoleDescription];

  // Details.
  if (_node->HasIntListAttribute(ax::mojom::IntListAttribute::kDetailsIds)) {
    [axAttributes addObject:NSAccessibilityDetailsElementsAttribute];
  }

  // Error messages.
  if (_node->HasIntListAttribute(
          ax::mojom::IntListAttribute::kErrormessageIds)) {
    [axAttributes addObject:NSAccessibilityErrorMessageElementsAttribute];
  }

  if (ui::SupportsRequired(role)) {
    [axAttributes addObject:NSAccessibilityRequiredAttribute];
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

  // Tree and grid (Outline role in Mac accessibility)
  if (ui::IsGridLike(role))
    [axAttributes addObject:NSAccessibilitySelectedRowsAttribute];

  // Popup
  if (_node->HasIntAttribute(ax::mojom::IntAttribute::kHasPopup)) {
    [axAttributes addObjectsFromArray:@[
      NSAccessibilityHasPopupAttribute, NSAccessibilityPopupValueAttribute
    ]];
  }

  // KeyShortcuts
  if (_node->HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts))
    [axAttributes addObject:NSAccessibilityKeyShortcutsValueAttribute];

  // TitleUIElement
  if ([self titleUIElement])
    [axAttributes addObject:NSAccessibilityTitleUIElementAttribute];

  return axAttributes;
}

// This API is deprecated.
// This method, while deprecated, is still called internally by AppKit.
- (NSArray*)accessibilityAttributeNames {
  TRACE_EVENT1("accessibility",
               "AXPlatformNodeCocoa::accessibilityAttributeNames",
               "role=", ui::ToString([self internalRole]));

  NSMutableArray* attributes = [self internalAccessibilityAttributeNames];

  // Exclude migrating attributes from being exposed.
  if (features::IsMacAccessibilityAPIMigrationEnabled()) {
    [attributes filterUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(
                                                      id evaluatedObject,
                                                      NSDictionary* bindings) {
                  return ![[self migratingAttributes]
                      containsObject:evaluatedObject];
                }]];
  }

  return attributes;
}

- (NSArray*)accessibilityParameterizedAttributeNames {
  if (!_node)
    return @[];

  // General attributes.
  NSMutableArray* ret = [NSMutableArray
      arrayWithObjects:
          NSAccessibilityAttributedStringForTextMarkerRangeParameterizedAttribute,
          nil];

  if (_node->HasState(ax::mojom::State::kEditable)) {
    [ret addObjectsFromArray:@[
      NSAccessibilityAttributedStringForRangeParameterizedAttribute
    ]];
  }
  return ret;
}

// Despite it being deprecated, AppKit internally calls this function sometimes
// in unclear circumstances. It is implemented in terms of the new a11y API
// here.
// This API is deprecated.
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  if (!_node)
    return;

  if ([self isMigratingAttribute:attribute]) {
    return;
  }

  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    [self setAccessibilityValue:value];
  } else if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute]) {
    [self setAccessibilitySelectedText:base::apple::ObjCCastStrict<NSString>(
                                           value)];
  } else if ([attribute
                 isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
    [self
        setAccessibilitySelectedTextRange:base::apple::ObjCCastStrict<NSValue>(
                                              value)
                                              .rangeValue];
  } else if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    [self setAccessibilityFocused:base::apple::ObjCCastStrict<NSNumber>(value)
                                      .boolValue];
  }
}

// This method, while deprecated, is still called internally by AppKit.
- (id)accessibilityAttributeValue:(NSString*)attribute {
  if (!_node)
    return nil;  // Return nil when detached. Even for ax::mojom::Role.

  SEL selector = NSSelectorFromString(attribute);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  if ([self respondsToSelector:selector])
    return [self performSelector:selector];
#pragma clang diagnostic pop
  return nil;
}

- (id)accessibilityAttributeValue:(NSString*)attribute
                     forParameter:(id)parameter {
  if (!_node)
    return nil;

  SEL selector = NSSelectorFromString([attribute stringByAppendingString:@":"]);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  if ([self respondsToSelector:selector])
    return [self performSelector:selector withObject:parameter];
#pragma clang diagnostic pop
  return nil;
}

//
// End of legacy deprecated NSAccessibility informal protocol.
//

// NSAccessibility (key-based) attributes. Order them according to
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
      NOTREACHED_IN_MIGRATION();
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

  NOTREACHED_IN_MIGRATION();
  return @"false";
}

- (NSNumber*)AXARIAColumnCount {
  if (![self instanceActive])
    return nil;
  std::optional<int> ariaColCount =
      _node->GetDelegate()->GetTableAriaColCount();
  if (!ariaColCount)
    return nil;
  return @(*ariaColCount);
}

- (NSNumber*)AXARIAColumnIndex {
  if (![self instanceActive])
    return nil;

  std::optional<int> ariaColIndex =
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
  std::optional<int> ariaRowCount =
      _node->GetDelegate()->GetTableAriaRowCount();
  if (!ariaRowCount)
    return nil;
  return @(*ariaRowCount);
}

- (NSNumber*)AXARIARowIndex {
  if (![self instanceActive])
    return nil;
  std::optional<int> ariaRowIndex =
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

- (NSString*)AXBrailleLabel {
  if (![self instanceActive])
    return nil;

  return
      [self getStringAttribute:ax::mojom::StringAttribute::kAriaBrailleLabel];
}

- (NSString*)AXBrailleRoleDescription {
  if (![self instanceActive])
    return nil;

  return [self getStringAttribute:ax::mojom::StringAttribute::
                                      kAriaBrailleRoleDescription];
}

- (id)AXBlockQuoteLevel {
  if (![self instanceActive])
    return nil;
  // This is for the number of ancestors that are a <blockquote>, including
  // self, useful for tracking replies to replies etc. in an email.
  int level = 0;

  for (ui::AXPlatformNodeBase* ancestor = _node; ancestor;
       ancestor = ancestor->GetPlatformParent()) {
    // Do not cross document boundaries.
    if (ui::IsPlatformDocument(ancestor->GetRole()))
      break;
    if (ancestor->GetRole() == ax::mojom::Role::kBlockquote)
      ++level;
  }
  return @(level);
}

- (NSArray*)AXColumnHeaderUIElements {
  return [self accessibilityColumnHeaderUIElements];
}

- (NSArray*)AXDetailsElements {
  if (![self instanceActive])
    return nil;

  NSMutableArray* elements = [NSMutableArray array];
  for (ui::AXNodeID id :
       _node->GetIntListAttribute(ax::mojom::IntListAttribute::kDetailsIds)) {
    AXPlatformNodeCocoa* node = [self fromNodeID:id];
    if (node)
      [elements addObject:node];
  }

  return elements.count ? elements : nil;
}

- (NSArray*)AXDOMClassList {
  if (![self instanceActive])
    return nil;

  NSMutableArray* ret = [NSMutableArray array];

  std::string classes;
  if (_node->GetStringAttribute(ax::mojom::StringAttribute::kClassName,
                                &classes)) {
    std::vector<std::string> split_classes = base::SplitString(
        classes, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& className : split_classes)
      [ret addObject:(base::SysUTF8ToNSString(className))];
  }
  return ret;
}

- (NSString*)AXDOMIdentifier {
  if (![self instanceActive])
    return nil;

  std::string id;
  if (_node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlId, &id)) {
    return base::SysUTF8ToNSString(id);
  }

  return @"";
}

- (id)AXEditableAncestor {
  if (![self instanceActive])
    return nil;
  ui::AXPlatformNodeBase* text_field_ancestor =
      _node->GetPlatformTextFieldAncestor();
  if (text_field_ancestor)
    return text_field_ancestor->GetNativeViewAccessible();
  return nil;
}

- (NSNumber*)AXElementBusy {
  if (![self instanceActive])
    return nil;
  return @(_node->GetBoolAttribute(ax::mojom::BoolAttribute::kBusy));
}

- (NSArray*)AXErrorMessageElements {
  if (![self instanceActive]) {
    return nil;
  }

  NSMutableArray* elements = [NSMutableArray array];
  for (ui::AXNodeID id : _node->GetIntListAttribute(
           ax::mojom::IntListAttribute::kErrormessageIds)) {
    AXPlatformNodeCocoa* node = [self fromNodeID:id];
    if (node) {
      [elements addObject:node];
    }
  }

  return elements.count ? elements : nil;
}

- (NSNumber*)AXGrabbed {
  return @NO;
}

- (NSNumber*)AXHasPopup {
  if (![self instanceActive])
    return nil;
  return @(_node->HasIntAttribute(ax::mojom::IntAttribute::kHasPopup));
}

- (id)AXHighestEditableAncestor {
  if (![self instanceActive])
    return nil;

  AXPlatformNodeCocoa* highestEditableAncestor = [self AXEditableAncestor];

  while (highestEditableAncestor) {
    AXPlatformNodeCocoa* ancestorParent = [highestEditableAncestor AXParent];
    if (!ancestorParent || ![ancestorParent isKindOfClass:[self class]])
      break;
    AXPlatformNodeCocoa* higherAncestor = [ancestorParent AXEditableAncestor];
    if (!higherAncestor)
      break;
    highestEditableAncestor = higherAncestor;
  }
  return highestEditableAncestor;
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

- (NSNumber*)AXIsMultiSelectable {
  if (![self instanceActive])
    return nil;
  return @(_node->HasState(ax::mojom::State::kMultiselectable));
}

- (NSString*)AXKeyShortcutsValue {
  if (![self instanceActive])
    return nil;
  return [self getStringAttribute:ax::mojom::StringAttribute::kKeyShortcuts];
}

- (NSNumber*)AXLoaded {
  if (![self instanceActive])
    return nil;
  return @(_node->GetDelegate()->GetTreeData().loaded);
}

- (NSNumber*)AXLoadingProgress {
  if (![self instanceActive])
    return nil;
  double doubleValue = _node->GetDelegate()->GetTreeData().loading_progress;
  return @(doubleValue);
}

- (id)AXOwns {
  if (![self instanceActive])
    return nil;

  ui::AXPlatformNodeBase* activeDescendant = _node->GetActiveDescendant();
  if (!activeDescendant)
    return nil;

  ui::AXPlatformNodeBase* container = activeDescendant->GetSelectionContainer();
  if (!container)
    return nil;

  return @[ container->GetNativeViewAccessible() ];
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
  return [self isAccessibilityRequired] ? @YES : @NO;
}

- (NSString*)AXRole {
  if (!_node)
    return nil;

  return [[self class] nativeRoleFromAXRole:_node->GetRole()];
}

- (NSString*)AXRoleDescription {
  return [self accessibilityRoleDescription];
}

- (NSNumber*)AXSelected {
  return [self accessibilitySelected];
}

- (NSArray*)AXSelectedRows {
  return [self accessibilitySelectedRows];
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

- (NSNumber*)AXVisited {
  if (![self instanceActive])
    return nil;

  return @(_node->HasState(ax::mojom::State::kVisited));
}

- (NSString*)AXHelp {
  if (![self instanceActive]) {
    return nil;
  }

  // ARIA descriptions are returned as AXCustomContent (see
  // -accessibilityCustomContent below), so if the description is from ARIA,
  // don't provide it as AXHelp, and return nothing.
  if ([self descriptionIsFromAriaDescription]) {
    return nil;
  }

  // Otherwise, it's a non-ARIA description, which is returned as AXHelp.
  return [self getStringAttribute:ax::mojom::StringAttribute::kDescription];
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
  return @([self isAccessibilityFocused]);
}

- (BOOL)isAccessibilityFocused {
  if (![self instanceActive]) {
    return NO;
  }

  return _node->GetDelegate()->GetFocus() == _node->GetNativeViewAccessible();
}

- (id)AXFocusableAncestor {
  if (![self instanceActive])
    return nil;

  ui::AXPlatformNodeBase* ancestor = _node;
  for (; ancestor; ancestor = ancestor->GetPlatformParent()) {
    // Do not cross document boundaries.
    if (ui::IsPlatformDocument(ancestor->GetRole()))
      return nil;
    if (ancestor->IsFocusable())
      break;
  }
  // The assignment to ancestor may be null.
  if (!ancestor)
    return nil;
  return ancestor->GetNativeViewAccessible();
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
    ui::AXPlatformNodeDelegate* child = child_iterator_ptr->get();
    if (child && child->IsInvisibleOrIgnored()) {
      [children
          addObjectsFromArray:[child_iterator_ptr->GetNativeViewAccessible()
                                  accessibilityChildren]];
    } else {
      [children addObject:child_iterator_ptr->GetNativeViewAccessible()];
    }
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
  return [self accessibilityTitle];
}

- (id)AXTitleUIElement {
  return [self accessibilityTitleUIElement];
}

- (NSString*)AXDescription {
  return [self accessibilityLabel];
}

// Misc attributes.

- (NSString*)AXPlaceholderValue {
  return [self accessibilityPlaceholderValue];
}

- (NSString*)AXMenuItemMarkChar {
  if (!ui::IsMenuItem(_node->GetRole()))
    return nil;

  const auto checkedState = static_cast<ax::mojom::CheckedState>(
      _node->GetIntAttribute(ax::mojom::IntAttribute::kCheckedState));
  if (checkedState == ax::mojom::CheckedState::kTrue) {
    return @"\u2713";  // "check mark"
  }

  return @"";
}

- (NSNumber*)AXARIAPosInSet {
  if (![self instanceActive])
    return nil;
  std::optional<int> posInSet = _node->GetPosInSet();
  if (!posInSet)
    return nil;
  return @(*posInSet);
}

- (NSNumber*)AXARIASetSize {
  if (![self instanceActive])
    return nil;
  std::optional<int> setSize = _node->GetSetSize();
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

  // TODO(crbug.com/41456329): Implement this for real.
  return [NSValue
      valueWithRange:NSMakeRange(0, [self accessibilityNumberOfCharacters])];
}

- (id)AXAttributedStringForRange:(id)parameter {
  if (![parameter isKindOfClass:[NSValue class]])
    return nil;

  // TODO(crbug.com/41456329): Finish implementation.
  // Currently, we only decorate the attributed string with misspelling
  // information.
  // TODO(tapted): views::WordLookupClient has a way to obtain the actual
  // decorations, and BridgedContentView has a conversion function that creates
  // an NSAttributedString. Refactor things so they can be used here.

  NSRange range = [(NSValue*)parameter rangeValue];
  std::u16string textContent = _node->GetTextContentUTF16();
  if (NSMaxRange(range) > textContent.length())
    return nil;

  // We potentially need to add text attributes to the whole text content
  // because a spelling mistake might start or end outside the given range.
  NSMutableAttributedString* attributedTextContent =
      [[NSMutableAttributedString alloc]
          initWithString:base::SysUTF16ToNSString(textContent)];
  if (!_node->IsText()) {
    AXRange axRange(_node->GetDelegate()->CreateTextPositionAt(0),
                    _node->GetDelegate()->CreateTextPositionAt(
                        static_cast<int>(textContent.length())));
    [self addTextAnnotationsIn:&axRange to:attributedTextContent];
  }

  return [attributedTextContent attributedSubstringFromRange:range];
}

- (NSAttributedString*)AXAttributedStringForTextMarkerRange:(id)markerRange {
  AXRange axRange = ui::AXTextMarkerRangeToAXRange(markerRange);
  if (axRange.IsNull())
    return nil;

  NSString* text = base::SysUTF16ToNSString(axRange.GetText());
  if (text.length == 0) {
    return nil;
  }

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:text];
  // Currently, we only decorate the attributed string with misspelling
  // and annotation information.
  [self addTextAnnotationsIn:&axRange to:attributedText];
  return attributedText;
}

- (NSString*)ChromeAXNodeId {
  return [@(_node->GetNodeId()) stringValue];
}

- (NSString*)description {
  return [NSString stringWithFormat:@"%@ - %@ (%@)", [super description],
                                    [self accessibilityTitle], [self AXRole]];
}

//
// End of key-based attributes.
//

//
// NSAccessibility protocol.
// https://developer.apple.com/documentation/appkit/nsaccessibilityprotocol
//

// These methods appear to be the minimum needed to avoid AppKit refusing to
// handle the element or crashing internally. Most of the remaining old API
// methods (the ones from NSObject) are implemented in terms of the new
// NSAccessibility methods.
//
// TODO(crbug.com/41115917): Does this class need to implement the various
// accessibilityPerformFoo methods, or are the stub implementations from
// NSAccessibilityElement sufficient?

// NSAccessibility: Configuring Accessibility.
- (BOOL)isAccessibilityElement {
  if (![self instanceActive])
    return NO;

  return (![[[self class] nativeRoleFromAXRole:_node->GetRole()]
              isEqualToString:NSAccessibilityUnknownRole] &&
          !_node->GetDelegate()->IsIgnored());
}

- (BOOL)isAccessibilityEnabled {
  if (!_node)
    return NO;

  return _node->GetData().GetRestriction() != ax::mojom::Restriction::kDisabled;
}

- (NSRect)accessibilityFrame {
  return [self boundsInScreen];
}

- (NSString*)accessibilityHelp {
  return [self AXHelp];
}

- (NSString*)accessibilityLabel {
  if (![self instanceActive])
    return nil;

  // macOS wants static text exposed in AXValue.
  if (ui::IsNameExposedInAXValueForRole([self internalRole]))
    return @"";

  // If we're exposing the title in TitleUIElement, don't also redundantly
  // expose it in accessibilityLabel.
  if ([self titleUIElement])
    return @"";

  if (![self isNameFromLabel])
    return @"";

  std::string name = _node->GetName();

  if (!name.empty())
    return base::SysUTF8ToNSString(name);

  // Given an image where there's no other title, return the base part
  // of the filename as the description.
  if ([self isImage]) {
    std::string url;
    if (_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl, &url)) {
      // Given a url like http://foo.com/bar/baz.png, just return the
      // base name, e.g., "baz.png".
      size_t leftIndex = url.rfind('/');
      std::string basename =
          leftIndex != std::string::npos ? url.substr(leftIndex) : url;
      return base::SysUTF8ToNSString(basename);
    }
  }

  return @"";
}

- (NSString*)accessibilityTitle {
  if (![self instanceActive])
    return nil;

  if (ui::IsNameExposedInAXValueForRole(_node->GetRole()))
    return @"";

  if ([self isNameFromLabel])
    return @"";

  // If we're exposing the title in TitleUIElement, don't also redundantly
  // expose it in AXDescription.
  if ([self titleUIElement])
    return @"";

  ax::mojom::NameFrom nameFrom = _node->GetNameFrom();

  // The accessible name, which is exposed via accessibilityTitle, should not
  // contain any placeholder text because an HTML or an ARIA placeholder refers
  // to a sample value that is usually found in a text field and is used to aid
  // the user in data entry. It is similar to a replacement for the value
  // attribute, not the title.
  if (nameFrom == ax::mojom::NameFrom::kPlaceholder)
    return @"";

  // Cell titles are empty if they came from content.
  if (nameFrom == ax::mojom::NameFrom::kContents) {
    NSString* role = [self accessibilityRole];
    if ([role isEqualToString:NSAccessibilityCellRole])
      return @"";
  }

  return [self getName];
}

- (id)accessibilityValue {
  return [self AXValue];
}

- (void)setAccessibilityValue:(id)value {
  if (!_node) {
    return;
  }

  ui::AXActionData data;
  data.action = _node->GetRole() == ax::mojom::Role::kTab
                    ? ax::mojom::Action::kSetSelection
                    : ax::mojom::Action::kSetValue;
  if ([value isKindOfClass:[NSString class]]) {
    data.value = base::SysNSStringToUTF8(value);
  } else if ([value isKindOfClass:[NSValue class]]) {
    // TODO(crbug.com/41115917): Is this case actually needed? The
    // NSObject accessibility implementation supported this, but can it actually
    // occur?
    NSRange range = [value rangeValue];
    data.anchor_offset = range.location;
    data.focus_offset = NSMaxRange(range);
  }
  _node->GetDelegate()->AccessibilityPerformAction(data);
}

- (BOOL)isAccessibilitySelectorAllowed:(SEL)selector {
  TRACE_EVENT1(
      "accessibility", "AXPlatformNodeCocoa::isAccessibilitySelectorAllowed",
      "selector=", base::SysNSStringToUTF8(NSStringFromSelector(selector)));

  if (!_node) {
    return NO;
  }

  if (selector == @selector(setAccessibilityFocused:)) {
    return _node->IsFocusable();
  }

  if (selector == @selector(setAccessibilityValue:)) {
    switch (_node->GetRole()) {
      case ax::mojom::Role::kSlider:
        // When VoiceOver performs an increment/decrement action, it immediately
        // calls upon success of the action the selector setAccessibilityValue
        // on the slider that was just updated. The value passed to this
        // function is always equals to 5% of the slider's value range, so
        // actually setting that value to our slider would:
        //   1. render the increment/decrement action performed a moment before
        //      useless as it would override the modified value;
        //   2. make the slider value stuck in place, at 5% of its range.
        //
        // I haven't found much on the topic online, so the following is at best
        // a conjecture: I believe that VoiceOver "suggests" us to
        // increment/decrement the value by 5%. There might be a setting I'm not
        // aware of that allows the VO users to modify this value by a different
        // one, which would allow them to always increment/decrement sliders by
        // the same amount on all apps.
        //
        // However, in Chromium, we handle the increment and decrement actions
        // on the blink side and the step value is computed over there. That
        // way, the experience for changing the value of a slider by increments
        // is the same for all different inputs: whether it's the keyboard arrow
        // keys, an AT, etc.
        //
        // TL;DR: setAccessibilityValue, when called on sliders, is breaking our
        // increment and decrement AX actions, so don't allow it.
        return NO;
      case ax::mojom::Role::kTab:
        // Tabs use the radio button role on Mac, so they are selected by
        // calling setSelected on an individual tab, rather than by setting the
        // selected element on the tabstrip as a whole.
        return !_node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
      default:
        break;
    }
  }

  // Don't allow calling AX setters on disabled elements.
  // TODO(crbug.com/41301942): Once the underlying bug in
  // views::Textfield::SetSelectionRange() described in that bug is fixed,
  // remove the check here when the selector is setAccessibilitySelectedText*;
  // right now, this check serves to prevent accessibility clients from trying
  // to set the selection range, which won't work because of 692362.
  if (_node->GetDelegate() && _node->GetDelegate()->IsReadOnlyOrDisabled() &&
      IsAXSetter(selector)) {
    return NO;
  }

  // TODO(crbug.com/41115917): What about role-specific selectors?
  return [super isAccessibilitySelectorAllowed:selector];
}

// NSAccessibility: Determining Relationships.
- (NSArray*)accessibilityChildren {
  return [self AXChildren];
}

- (id)accessibilityParent {
  return [self AXParent];
}

// NSAccessibility: Assigning Roles.
- (BOOL)isAccessibilityRequired {
  TRACE_EVENT1("accessibility", "accessibilityRequired",
               "role=", ui::ToString([self internalRole]));

  if (![self instanceActive]) {
    return NO;
  }

  return _node->HasState(ax::mojom::State::kRequired);
}

- (NSAccessibilityRole)accessibilityRole {
  return [self AXRole];
}

- (NSAccessibilitySubrole)accessibilitySubrole {
  return [self AXSubrole];
}

- (NSString*)accessibilityRoleDescription {
  TRACE_EVENT1("accessibility", "accessibilityRoleDescription",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive]) {
    return nil;
  }

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
  switch ([self internalRole]) {
    case ax::mojom::Role::kColorWell:            // Use platform's "color well"
    case ax::mojom::Role::kImage:                // Default: IDS_AX_ROLE_GRAPHIC
    case ax::mojom::Role::kInputTime:            // Use platform's "time field"
    case ax::mojom::Role::kMeter:        // Use platform's "level indicator"
    case ax::mojom::Role::kPopUpButton:  // Use platform's "popup button"
    case ax::mojom::Role::kTabList:      // Use platform's "tab group"
    case ax::mojom::Role::kTree:         // Use platform's "outline"
    case ax::mojom::Role::kTreeItem:     // Use platform's "outline row"
      break;
    case ax::mojom::Role::kHeader:  // Default: IDS_AX_ROLE_HEADER
      return l10n_util::GetNSString(IDS_AX_ROLE_BANNER);
    case ax::mojom::Role::kRootWebArea: {
      if ([role isEqualToString:NSAccessibilityWebAreaRole]) {
        return l10n_util::GetNSString(IDS_AX_ROLE_WEB_AREA);
      }
      // Preserve platform default of "group" in the case of the child
      // of a presentational <iframe> which has the internal role of
      // kRootWebArea.
      break;
    }
    default: {
      std::u16string result =
          _node->GetDelegate()->GetLocalizedStringForRoleDescription();
      if (!result.empty()) {
        return base::SysUTF16ToNSString(result);
      }
    }
  }

  return NSAccessibilityRoleDescription(role, [self accessibilitySubrole]);
}

// NSAccessibility: Configuring Table and Outline Views.
- (NSArray*)accessibilitySelectedRows {
  if (![self instanceActive]) {
    return nil;
  }

  NSArray* rows = [self accessibilityRows];
  // accessibilityRows returns an empty array unless instanceActive does,
  // not exist, so we do not need to check if rows is nil at this time.
  NSMutableArray* selectedRows = [NSMutableArray array];
  for (id row in rows) {
    if ([[row accessibilitySelected] boolValue]) {
      [selectedRows addObject:row];
    }
  }
  return selectedRows;
}

- (NSArray*)accessibilityColumnHeaderUIElements {
  if (![self instanceActive]) {
    return nil;
  }

  ui::AXPlatformNodeDelegate* delegate = _node->GetDelegate();
  DCHECK(delegate);

  NSMutableArray* ret = [NSMutableArray array];

  // If this is a table, return all column headers.
  ax::mojom::Role role = _node->GetRole();
  if (ui::IsTableLike(role)) {
    for (ui::AXNodeID id : delegate->GetColHeaderNodeIds()) {
      AXPlatformNodeCocoa* colheader = [self fromNodeID:id];
      if (colheader) {
        [ret addObject:colheader];
      }
    }
    return [ret count] ? ret : nil;
  }

  // Otherwise if this is a cell or a header cell, return the column headers for
  // it.
  if (!ui::IsCellOrTableHeader(role)) {
    return nil;
  }

  ui::AXPlatformNodeBase* table = _node->GetTable();
  if (!table) {
    return nil;
  }

  std::optional<int> column = delegate->GetTableCellColIndex();
  if (!column) {
    return nil;
  }

  ui::AXPlatformNodeDelegate* tableDelegate = table->GetDelegate();
  DCHECK(tableDelegate);
  for (ui::AXNodeID id : tableDelegate->GetColHeaderNodeIds(*column)) {
    AXPlatformNodeCocoa* colheader = [self fromNodeID:id];
    if (colheader) {
      [ret addObject:colheader];
    }
  }
  return [ret count] ? ret : nil;
}

- (NSArray*)accessibilityRows {
  // TODO(accessibility) accessibilityRows is defined in
  // browser_accessibility_cocoa.mm eventually that function definition should
  // be moved here.
  return nil;
}

// NSAccessibility: Setting the Focus.
- (void)setAccessibilityFocused:(BOOL)isFocused {
  if (!_node)
    return;

  ui::AXActionData data;
  data.action =
      isFocused ? ax::mojom::Action::kFocus : ax::mojom::Action::kBlur;
  _node->GetDelegate()->AccessibilityPerformAction(data);
}

// NSAccessibility: Configuring Text Elements.

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
    return [self getName];

  return [self getStringAttribute:ax::mojom::StringAttribute::kPlaceholder];
}

- (NSString*)accessibilitySelectedText {
  if (!_node)
    return nil;

  return [self AXSelectedText];
}

- (void)setAccessibilitySelectedText:(NSString*)text {
  if (!_node) {
    return;
  }

  ui::AXActionData data;
  data.action = ax::mojom::Action::kReplaceSelectedText;
  data.value = base::SysNSStringToUTF8(text);

  _node->GetDelegate()->AccessibilityPerformAction(data);
}

- (NSRange)accessibilitySelectedTextRange {
  if (!_node)
    return NSMakeRange(0, 0);

  NSRange r;
  [[self AXSelectedTextRange] getValue:&r];
  return r;
}

- (void)setAccessibilitySelectedTextRange:(NSRange)range {
  if (!_node) {
    return;
  }

  ui::AXActionData data;
  data.action = ax::mojom::Action::kSetSelection;
  data.anchor_offset = range.location;
  data.anchor_node_id = _node->GetData().id;
  data.focus_offset = NSMaxRange(range);
  data.focus_node_id = _node->GetData().id;
  _node->GetDelegate()->AccessibilityPerformAction(data);
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

  return [[self AXLineForIndex:@(index)] integerValue];
}

- (NSRange)accessibilityRangeForIndex:(NSInteger)index {
  if (!_node)
    return NSMakeRange(0, 0);

  return [[self AXRangeForIndex:@(index)] rangeValue];
}

- (NSRange)accessibilityStyleRangeForIndex:(NSInteger)index {
  if (!_node)
    return NSMakeRange(0, 0);

  return [[self AXStyleRangeForIndex:@(index)] rangeValue];
}

- (NSRange)accessibilityRangeForLine:(NSInteger)line {
  if (!_node)
    return NSMakeRange(0, 0);

  return [[self AXRangeForLine:@(line)] rangeValue];
}

- (NSRange)accessibilityRangeForPosition:(NSPoint)point {
  return [[self AXRangeForPosition:[NSValue valueWithPoint:point]] rangeValue];
}

// NSAccessibility: setting content and values.
- (NSNumber*)accessibilitySelected {
  if (![self instanceActive])
    return nil;

  return @(_node->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

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

// NSAccessibility: configuring linkage elements.
- (id)accessibilityTitleUIElement {
  if (![self instanceActive])
    return nil;

  return [self titleUIElement];
}

//
// End of NSAccessibility protocol.
//

//
// AXCustomContentProvider
// https://developer.apple.com/documentation/accessibility/axcustomcontentprovider/3600104-accessibilitycustomcontent
//

- (NSArray*)accessibilityCustomContent {
  if (![self instanceActive]) {
    return nil;
  }

  // Only descriptions originating from ARIA are returned as custom content.
  // (Non-ARIA descriptions are returned as AXHelp.)
  if (![self descriptionIsFromAriaDescription]) {
    return nil;
  }

  NSString* description =
      [self getStringAttribute:ax::mojom::StringAttribute::kDescription];
  AXCustomContent* contentItem =
      [AXCustomContent customContentWithLabel:@"description" value:description];
  // A custom content importance of high causes it to be spoken
  // automatically, rather than "More content available".
  contentItem.importance = AXCustomContentImportanceHigh;
  return @[ contentItem ];
}

// MathML attributes.
// TODO(crbug.com/40673555): The MathML aam considers only in-flow children.
// TODO(crbug.com/40673555): When/if it is needed to expose this for other a11y
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

namespace {

NSDictionary* CreateMathSubSupScriptsPair(AXPlatformNodeCocoa* subscript,
                                          AXPlatformNodeCocoa* superscript) {
  NSMutableDictionary* dictionary = [NSMutableDictionary dictionary];
  if (subscript) {
    dictionary[NSAccessibilityMathSubscriptAttribute] = subscript;
  }
  if (superscript) {
    dictionary[NSAccessibilityMathSuperscriptAttribute] = superscript;
  }
  return dictionary;
}

}  // namespace

- (NSArray*)AXMathPostscripts {
  if (![self instanceActive] ||
      _node->GetRole() != ax::mojom::Role::kMathMLMultiscripts)
    return nil;
  NSMutableArray* ret = [NSMutableArray array];
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
    [ret addObject:CreateMathSubSupScriptsPair(subscript, superscript)];
    subscript = nullptr;
  }
  return [ret count] ? ret : nil;
}

- (NSArray*)AXMathPrescripts {
  if (![self instanceActive] ||
      _node->GetRole() != ax::mojom::Role::kMathMLMultiscripts)
    return nil;
  NSMutableArray* ret = [NSMutableArray array];
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
    [ret addObject:CreateMathSubSupScriptsPair(subscript, superscript)];
    subscript = nullptr;
  }
  return [ret count] ? ret : nil;
}

@end
