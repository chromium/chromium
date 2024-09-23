// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_manager_win.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/typed_macros.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "ui/accessibility/platform/browser_accessibility_win.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_fragment_root_win.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/accessibility/platform/ax_platform_node_delegate_utils_win.h"
#include "ui/accessibility/platform/ax_platform_node_textprovider_win.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/accessibility/platform/uia_registrar_win.h"
#include "ui/base/win/atl_module.h"

namespace ui {

namespace {

#if DCHECK_IS_ON()
#define DCHECK_IN_ON_ACCESSIBILITY_EVENTS()                                \
  DCHECK(in_on_accessibility_events_)                                      \
      << "This method should only be called during OnAccessibilityEvents " \
         "because memoized information is cleared afterwards in "          \
         "FinalizeAccessibilityEvents"
#else
#define DCHECK_IN_ON_ACCESSIBILITY_EVENTS()
#endif  // DCHECK_IS_ON()

BrowserAccessibility* GetUiaTextPatternProvider(BrowserAccessibility& node) {
  for (BrowserAccessibility* current_node = &node; current_node;
       current_node = current_node->PlatformGetParent()) {
    if (ToBrowserAccessibilityWin(current_node)
            ->GetCOM()
            ->IsPatternProviderSupported(UIA_TextPatternId)) {
      return current_node;
    }
  }

  return nullptr;
}

}  // namespace

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const AXTreeUpdate& initial_tree,
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate) {
  return new BrowserAccessibilityManagerWin(initial_tree, node_id_delegate,
                                            delegate);
}

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate) {
  return new BrowserAccessibilityManagerWin(
      BrowserAccessibilityManagerWin::GetEmptyDocument(), node_id_delegate,
      delegate);
}

BrowserAccessibilityManagerWin*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerWin() {
  return static_cast<BrowserAccessibilityManagerWin*>(this);
}

BrowserAccessibilityManagerWin::BrowserAccessibilityManagerWin(
    const AXTreeUpdate& initial_tree,
    AXNodeIdDelegate& node_id_delegate,
    AXPlatformTreeManagerDelegate* delegate)
    : BrowserAccessibilityManager(node_id_delegate, delegate) {
  win::CreateATLModuleIfNeeded();
  Initialize(initial_tree);
}

BrowserAccessibilityManagerWin::~BrowserAccessibilityManagerWin() {
  // In some cases, an iframe's HWND is destroyed before the hypertext
  // on the parent child tree owner is destroyed. In this case, we reset
  // the parent's hypertext to avoid API calls involving stale hypertext.
  BrowserAccessibility* parent =
      GetParentNodeFromParentTreeAsBrowserAccessibility();
  if (!parent) {
    return;
  }

  AXPlatformNode* parent_ax_platform_node = parent->GetAXPlatformNode();
  if (!parent_ax_platform_node) {
    return;
  }

  static_cast<AXPlatformNodeWin*>(parent_ax_platform_node)
      ->ResetComputedHypertext();
}

// static
AXTreeUpdate BrowserAccessibilityManagerWin::GetEmptyDocument() {
  AXNodeData empty_document;
  empty_document.id = kInitialEmptyDocumentRootNodeID;
  empty_document.role = ax::mojom::Role::kRootWebArea;
  empty_document.AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, true);
  AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

HWND BrowserAccessibilityManagerWin::GetParentHWND() const {
  AXPlatformTreeManagerDelegate* delegate = GetDelegateFromRootManager();
  if (!delegate)
    return NULL;
  return delegate->AccessibilityGetAcceleratedWidget();
}

void BrowserAccessibilityManagerWin::UserIsReloading() {
  if (GetBrowserAccessibilityRoot())
    FireWinAccessibilityEvent(IA2_EVENT_DOCUMENT_RELOAD,
                              GetBrowserAccessibilityRoot());
}

void BrowserAccessibilityManagerWin::FireAriaNotificationEvent(
    BrowserAccessibility* node,
    const std::string& announcement,
    const std::string& notification_id,
    ax::mojom::AriaNotificationInterrupt interrupt_property,
    ax::mojom::AriaNotificationPriority priority_property) {
  DCHECK(node);

  // This API is only supported from Windows10 (version 1709) onwards.
  // Check if the function pointer is valid or not.
  using UiaRaiseNotificationEventFunction =
      HRESULT(WINAPI*)(IRawElementProviderSimple*, NotificationKind,
                       NotificationProcessing, BSTR, BSTR);
  static const UiaRaiseNotificationEventFunction
      uia_raise_notification_event_func =
          reinterpret_cast<UiaRaiseNotificationEventFunction>(
              ::GetProcAddress(GetModuleHandle(L"uiautomationcore.dll"),
                               "UiaRaiseNotificationEvent"));
  if (!uia_raise_notification_event_func) {
    return;
  }

  auto MapPropertiesToUiaNotificationProcessing =
      [&]() -> NotificationProcessing {
    switch (interrupt_property) {
      case ax::mojom::AriaNotificationInterrupt::kNone:
        switch (priority_property) {
          case ax::mojom::AriaNotificationPriority::kNone:
            return NotificationProcessing_All;
          case ax::mojom::AriaNotificationPriority::kImportant:
            return NotificationProcessing_ImportantAll;
        }
      case ax::mojom::AriaNotificationInterrupt::kAll:
        switch (priority_property) {
          case ax::mojom::AriaNotificationPriority::kNone:
            return NotificationProcessing_MostRecent;
          case ax::mojom::AriaNotificationPriority::kImportant:
            return NotificationProcessing_ImportantMostRecent;
        }
      case ax::mojom::AriaNotificationInterrupt::kPending:
        switch (priority_property) {
          case ax::mojom::AriaNotificationPriority::kNone:
            return NotificationProcessing_CurrentThenMostRecent;
          case ax::mojom::AriaNotificationPriority::kImportant:
            // This is resolved the same as `AriaNotificationInterrupt::kAll`,
            // but UIA doesn't have a specific enum value for these options yet.
            return NotificationProcessing_ImportantMostRecent;
        }
    }
    NOTREACHED();
  };

  const base::win::ScopedBstr announcement_bstr(base::UTF8ToWide(announcement));
  const base::win::ScopedBstr notification_id_bstr(
      base::UTF8ToWide(notification_id));

  uia_raise_notification_event_func(ToBrowserAccessibilityWin(node)->GetCOM(),
                                    NotificationKind_ActionCompleted,
                                    MapPropertiesToUiaNotificationProcessing(),
                                    announcement_bstr.Get(),
                                    notification_id_bstr.Get());
}

void BrowserAccessibilityManagerWin::FireFocusEvent(AXNode* node) {
  AXTreeManager::FireFocusEvent(node);
  DCHECK(node);
  BrowserAccessibility* wrapper = GetFromAXNode(node);
  FireWinAccessibilityEvent(EVENT_OBJECT_FOCUS, wrapper);
  FireUiaAccessibilityEvent(UIA_AutomationFocusChangedEventId, wrapper);
}

void BrowserAccessibilityManagerWin::FireBlinkEvent(ax::mojom::Event event_type,
                                                    BrowserAccessibility* node,
                                                    int action_request_id) {
  DCHECK(CanFireEvents());

  BrowserAccessibilityManager::FireBlinkEvent(event_type, node,
                                              action_request_id);

  switch (event_type) {
    case ax::mojom::Event::kClicked:
      if (node->GetData().IsInvocable())
        FireUiaAccessibilityEvent(UIA_Invoke_InvokedEventId, node);
      break;
    case ax::mojom::Event::kEndOfTest:
      // Event tests use kEndOfTest as a sentinel to mark the end of the test.
      FireUiaAccessibilityEvent(
          UiaRegistrarWin::GetInstance().GetTestCompleteEventId(), node);
      break;
    case ax::mojom::Event::kLoadComplete:
      FireWinAccessibilityEvent(IA2_EVENT_DOCUMENT_LOAD_COMPLETE, node);
      FireUiaAccessibilityEvent(UIA_AsyncContentLoadedEventId, node);
      break;
    case ax::mojom::Event::kLocationChanged:
      FireWinAccessibilityEvent(IA2_EVENT_VISIBLE_DATA_CHANGED, node);
      break;
    case ax::mojom::Event::kScrolledToAnchor: {
      FireWinAccessibilityEvent(EVENT_SYSTEM_SCROLLINGSTART, node);
      FireUiaActiveTextPositionChangedEvent(node);
      break;
    }
    case ax::mojom::Event::kTextChanged:
      // TODO(crbug.com/40672441) Remove when Views are exposed in the AXTree
      // which will fire generated text-changed events.
      if (!node->IsWebContent())
        EnqueueTextChangedEvent(*node);
      break;
    default:
      break;
  }
}

void BrowserAccessibilityManagerWin::FireGeneratedEvent(
    AXEventGenerator::Event event_type,
    const AXNode* node) {
  BrowserAccessibilityManager::FireGeneratedEvent(event_type, node);
  BrowserAccessibility* wrapper = GetFromAXNode(node);
  DCHECK(wrapper);
  switch (event_type) {
    case AXEventGenerator::Event::ACCESS_KEY_CHANGED:
      FireUiaPropertyChangedEvent(UIA_AccessKeyPropertyId, wrapper);
      break;
    case AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED:
      FireWinAccessibilityEvent(IA2_EVENT_ACTIVE_DESCENDANT_CHANGED, wrapper);
      break;
    case AXEventGenerator::Event::ALERT:
      FireWinAccessibilityEvent(EVENT_SYSTEM_ALERT, wrapper);
      // Generated 'ALERT' events come from role=alert nodes in the tree.
      // These should just be treated as normal live region changed events,
      // since we don't want web pages to be performing system-wide alerts.
      FireUiaAccessibilityEvent(UIA_LiveRegionChangedEventId, wrapper);
      break;
    case AXEventGenerator::Event::ATOMIC_CHANGED:
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::BUSY_CHANGED:
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::CHECKED_STATE_CHANGED:
      // https://www.w3.org/TR/core-aam-1.1/#mapping_state-property_table
      // SelectionItem.IsSelected is set according to the True or False value of
      // aria-checked for 'radio' and 'menuitemradio' roles.
      if (IsRadio(wrapper->GetRole())) {
        HandleSelectedStateChanged(uia_selection_events_, wrapper,
                                   IsUIANodeSelected(wrapper));
      }
      FireUiaPropertyChangedEvent(UIA_ToggleToggleStatePropertyId, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::CHILDREN_CHANGED: {
      // If this node is ignored, fire the event on the platform parent since
      // ignored nodes cannot raise events.
      BrowserAccessibility* target_node =
          wrapper->IsIgnored() ? wrapper->PlatformGetParent() : wrapper;
      if (target_node) {
        FireWinAccessibilityEvent(EVENT_OBJECT_REORDER, target_node);
        FireUiaStructureChangedEvent(StructureChangeType_ChildrenReordered,
                                     target_node);
      }
      break;
    }
    case AXEventGenerator::Event::COLLAPSED:
    case AXEventGenerator::Event::EXPANDED:
      FireUiaPropertyChangedEvent(
          UIA_ExpandCollapseExpandCollapseStatePropertyId, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::CONTROLS_CHANGED:
      FireUiaPropertyChangedEvent(UIA_ControllerForPropertyId, wrapper);
      break;
    case AXEventGenerator::Event::DESCRIBED_BY_CHANGED:
      FireUiaPropertyChangedEvent(UIA_DescribedByPropertyId, wrapper);
      break;
    case AXEventGenerator::Event::DESCRIPTION_CHANGED:
      FireUiaPropertyChangedEvent(UIA_FullDescriptionPropertyId, wrapper);
      break;
    case AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED: {
      // Fire the event on the object where the focus of the selection is. This
      // is because the focus is the only endpoint that can move, and because
      // the caret (if present) is at the focus. Since this is a focus-related
      // event (and focused nodes are never ignored), we can query the focused
      // node directly from the AXTree, and avoid calling GetUnignoredSelection.
      AXNodeID focus_id = ax_tree()->data().sel_focus_object_id;
      BrowserAccessibility* focus_object = GetFromID(focus_id);
      if (focus_object) {
        EnqueueSelectionChangedEvent(*focus_object);
        if (BrowserAccessibility* text_field =
                focus_object->PlatformGetTextFieldAncestor()) {
          EnqueueSelectionChangedEvent(*text_field);

          // Atomic text fields (including input and textarea elements) have
          // descendant objects that are part of their internal implementation
          // in Blink, which are not exposed to platform APIs in the
          // accessibility tree. Firing an event on such descendants will not
          // reach the assistive software.
          if (text_field->IsAtomicTextField()) {
            FireWinAccessibilityEvent(IA2_EVENT_TEXT_CARET_MOVED, text_field);
          } else {
            FireWinAccessibilityEvent(IA2_EVENT_TEXT_CARET_MOVED, focus_object);
          }
        } else {
          // Fire the event on the root object, which in the absence of a text
          // field ancestor is the closest UIA text provider (other than the
          // focused object) in which the selection has changed.
          DCHECK(IsPlatformDocument(wrapper->GetRole()));
          EnqueueSelectionChangedEvent(*wrapper);

          // "IA2_EVENT_TEXT_CARET_MOVED" should only be fired when a visible
          // caret or a selection is present. In the case of a text field above,
          // this is implicitly true.
          if (wrapper->HasVisibleCaretOrSelection())
            FireWinAccessibilityEvent(IA2_EVENT_TEXT_CARET_MOVED, focus_object);
        }
      }
      break;
    }
    case AXEventGenerator::Event::EDITABLE_TEXT_CHANGED:
      EnqueueTextChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::ENABLED_CHANGED:
      FireUiaPropertyChangedEvent(UIA_IsEnabledPropertyId, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::FLOW_FROM_CHANGED:
      FireUiaPropertyChangedEvent(UIA_FlowsFromPropertyId, wrapper);
      break;
    case AXEventGenerator::Event::FLOW_TO_CHANGED:
      FireUiaPropertyChangedEvent(UIA_FlowsToPropertyId, wrapper);
      break;
    case AXEventGenerator::Event::HASPOPUP_CHANGED:
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED:
      FireUiaPropertyChangedEvent(UIA_LevelPropertyId, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::IGNORED_CHANGED:
      if (wrapper->IsIgnored()) {
        FireWinAccessibilityEvent(EVENT_OBJECT_HIDE, wrapper);
        FireUiaStructureChangedEvent(StructureChangeType_ChildRemoved, wrapper);
      }
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_NAMECHANGE, wrapper);
      break;
    case AXEventGenerator::Event::INVALID_STATUS_CHANGED:
      FireUiaPropertyChangedEvent(UIA_IsDataValidForFormPropertyId, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::ARIA_CURRENT_CHANGED:
      // TODO(accessibility) No UIA mapping yet exists for aria-current.
      // Request a mapping from API owners and implement.
      FireWinAccessibilityEvent(IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED, wrapper);
      break;
    case AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED:
      FireUiaPropertyChangedEvent(UIA_AcceleratorKeyPropertyId, wrapper);
      break;
    case AXEventGenerator::Event::LABELED_BY_CHANGED:
      FireUiaPropertyChangedEvent(UIA_LabeledByPropertyId, wrapper);
      break;
    case AXEventGenerator::Event::LANGUAGE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_CulturePropertyId, wrapper);
      break;
    case AXEventGenerator::Event::LIVE_REGION_CHANGED:
      // This event is redundant with the IA2_EVENT_TEXT_INSERTED events;
      // however, JAWS 2018 and earlier do not process the text inserted
      // events when "virtual cursor mode" is turned off (Insert+Z).
      // Fortunately, firing the redudant event does not cause duplicate
      // verbalizations in either screen reader.
      // Future versions of JAWS may process the text inserted event when
      // in focus mode, and so at some point the live region
      // changed events may truly become redundant with the text inserted
      // events. Note: Firefox does not fire this event, but JAWS processes
      // Firefox live region events differently (utilizes MSAA's
      // EVENT_OBJECT_SHOW).
      FireWinAccessibilityEvent(EVENT_OBJECT_LIVEREGIONCHANGED, wrapper);
      FireUiaAccessibilityEvent(UIA_LiveRegionChangedEventId, wrapper);
      break;
    case AXEventGenerator::Event::LIVE_RELEVANT_CHANGED:
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::LIVE_STATUS_CHANGED:
      FireUiaPropertyChangedEvent(UIA_LiveSettingPropertyId, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::LAYOUT_INVALIDATED:
      FireUiaAccessibilityEvent(UIA_LayoutInvalidatedEventId, wrapper);
      break;
    case AXEventGenerator::Event::MENU_POPUP_END:
      FireWinAccessibilityEvent(EVENT_SYSTEM_MENUPOPUPEND, wrapper);
      FireUiaAccessibilityEvent(UIA_MenuClosedEventId, wrapper);
      break;
    case AXEventGenerator::Event::MENU_POPUP_START:
      FireWinAccessibilityEvent(EVENT_SYSTEM_MENUPOPUPSTART, wrapper);
      FireUiaAccessibilityEvent(UIA_MenuOpenedEventId, wrapper);
      break;
    case AXEventGenerator::Event::MULTILINE_STATE_CHANGED:
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::MULTISELECTABLE_STATE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_SelectionCanSelectMultiplePropertyId,
                                  wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::NAME_CHANGED:
      if (wrapper->IsText()) {
        EnqueueTextChangedEvent(*wrapper);
      } else {
        FireUiaPropertyChangedEvent(UIA_NamePropertyId, wrapper);
      }
      // Only fire name changes when the name comes from an attribute, otherwise
      // name changes are redundant with text removed/inserted events.
      if (wrapper->GetNameFrom() != ax::mojom::NameFrom::kContents)
        FireWinAccessibilityEvent(EVENT_OBJECT_NAMECHANGE, wrapper);
      break;
    case AXEventGenerator::Event::OBJECT_ATTRIBUTE_CHANGED:
      FireWinAccessibilityEvent(IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED, wrapper);
      // TODO(crbug.com/40707706): Fire UIA event.
      break;
    case AXEventGenerator::Event::PLACEHOLDER_CHANGED:
      FireUiaPropertyChangedEvent(UIA_HelpTextPropertyId, wrapper);
      break;
    case AXEventGenerator::Event::POSITION_IN_SET_CHANGED:
      FireUiaPropertyChangedEvent(UIA_PositionInSetPropertyId, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::RANGE_VALUE_CHANGED:
      DCHECK(wrapper->GetData().IsRangeValueSupported());
      FireWinAccessibilityEvent(EVENT_OBJECT_VALUECHANGE, wrapper);
      FireUiaPropertyChangedEvent(UIA_RangeValueValuePropertyId, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::RANGE_VALUE_MAX_CHANGED:
      DCHECK(wrapper->GetData().IsRangeValueSupported());
      FireUiaPropertyChangedEvent(UIA_RangeValueMaximumPropertyId, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::RANGE_VALUE_MIN_CHANGED:
      DCHECK(wrapper->GetData().IsRangeValueSupported());
      FireUiaPropertyChangedEvent(UIA_RangeValueMinimumPropertyId, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::RANGE_VALUE_STEP_CHANGED:
      DCHECK(wrapper->GetData().IsRangeValueSupported());
      FireUiaPropertyChangedEvent(UIA_RangeValueSmallChangePropertyId, wrapper);
      FireUiaPropertyChangedEvent(UIA_RangeValueLargeChangePropertyId, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::READONLY_CHANGED:
      if (wrapper->GetData().IsRangeValueSupported())
        FireUiaPropertyChangedEvent(UIA_RangeValueIsReadOnlyPropertyId,
                                    wrapper);
      else if (IsValuePatternSupported(wrapper)) {
        FireUiaPropertyChangedEvent(UIA_ValueIsReadOnlyPropertyId, wrapper);
      }
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::REQUIRED_STATE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_IsRequiredForFormPropertyId, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::ROLE_CHANGED:
      FireWinAccessibilityEvent(IA2_EVENT_ROLE_CHANGED, wrapper);
      FireUiaPropertyChangedEvent(UIA_AriaRolePropertyId, wrapper);
      break;
    case AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED:
      FireWinAccessibilityEvent(EVENT_SYSTEM_SCROLLINGEND, wrapper);
      FireUiaPropertyChangedEvent(UIA_ScrollHorizontalScrollPercentPropertyId,
                                  wrapper);
      break;
    case AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED:
      FireWinAccessibilityEvent(EVENT_SYSTEM_SCROLLINGEND, wrapper);
      FireUiaPropertyChangedEvent(UIA_ScrollVerticalScrollPercentPropertyId,
                                  wrapper);
      break;
    case AXEventGenerator::Event::SELECTED_CHANGED:
      HandleSelectedStateChanged(ia2_selection_events_, wrapper,
                                 IsIA2NodeSelected(wrapper));
      HandleSelectedStateChanged(uia_selection_events_, wrapper,
                                 IsUIANodeSelected(wrapper));
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_SELECTIONWITHIN, wrapper);
      break;
    case AXEventGenerator::Event::SELECTED_VALUE_CHANGED:
      DCHECK(IsSelectElement(wrapper->GetRole()));
      FireWinAccessibilityEvent(EVENT_OBJECT_VALUECHANGE, wrapper);
      FireUiaPropertyChangedEvent(UIA_ValueValuePropertyId, wrapper);
      // By changing the value of a combo box, the document's text contents will
      // also have changed.
      EnqueueTextChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::SET_SIZE_CHANGED:
      FireUiaPropertyChangedEvent(UIA_SizeOfSetPropertyId, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::SORT_CHANGED:
      FireWinAccessibilityEvent(IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED, wrapper);
      HandleAriaPropertiesChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::SUBTREE_CREATED:
      FireWinAccessibilityEvent(EVENT_OBJECT_SHOW, wrapper);
      FireUiaStructureChangedEvent(StructureChangeType_ChildAdded, wrapper);
      break;
    case AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED:
      FireWinAccessibilityEvent(IA2_EVENT_TEXT_ATTRIBUTE_CHANGED, wrapper);
      break;
    case AXEventGenerator::Event::VALUE_IN_TEXT_FIELD_CHANGED:
      DCHECK(wrapper->IsTextField());
      FireWinAccessibilityEvent(EVENT_OBJECT_VALUECHANGE, wrapper);
      FireUiaPropertyChangedEvent(UIA_ValueValuePropertyId, wrapper);
      EnqueueTextChangedEvent(*wrapper);
      break;
    case AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED:
      FireWinAccessibilityEvent(EVENT_OBJECT_STATECHANGE, wrapper);
      break;

    // Currently unused events on this platform.
    case AXEventGenerator::Event::NONE:
    case AXEventGenerator::Event::ARIA_NOTIFICATIONS_POSTED:
    case AXEventGenerator::Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED:
    case AXEventGenerator::Event::AUTO_COMPLETE_CHANGED:
    case AXEventGenerator::Event::AUTOFILL_AVAILABILITY_CHANGED:
    case AXEventGenerator::Event::CARET_BOUNDS_CHANGED:
    case AXEventGenerator::Event::CHECKED_STATE_DESCRIPTION_CHANGED:
    case AXEventGenerator::Event::DETAILS_CHANGED:
    case AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED:
    case AXEventGenerator::Event::FOCUS_CHANGED:
    case AXEventGenerator::Event::LIVE_REGION_CREATED:
    case AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED:
    case AXEventGenerator::Event::MENU_ITEM_SELECTED:
    case AXEventGenerator::Event::ORIENTATION_CHANGED:
    case AXEventGenerator::Event::PARENT_CHANGED:
    case AXEventGenerator::Event::RELATED_NODE_CHANGED:
    case AXEventGenerator::Event::ROW_COUNT_CHANGED:
    case AXEventGenerator::Event::STATE_CHANGED:
    case AXEventGenerator::Event::TEXT_SELECTION_CHANGED:
      break;
  }
}

void BrowserAccessibilityManagerWin::FireWinAccessibilityEvent(
    LONG win_event_type,
    BrowserAccessibility* node) {
  TRACE_EVENT("accessibility", "FireWinAccessibilityEvent");
  if (!ShouldFireEventForNode(node))
    return;
  // Suppress events when |IGNORED_CHANGED| except for related SHOW / HIDE.
  // Also include MENUPOPUPSTART / MENUPOPUPEND since a change in the ignored
  // state may show / hide a popup by exposing it to the tree or not.
  // Also include focus events since a node may become visible at the same time
  // it receives focus It's never good to suppress a po
  if (IsIgnoredChangedNode(node)) {
    switch (win_event_type) {
      case EVENT_OBJECT_HIDE:
      case EVENT_OBJECT_SHOW:
      case EVENT_OBJECT_FOCUS:
      case EVENT_SYSTEM_MENUPOPUPEND:
      case EVENT_SYSTEM_MENUPOPUPSTART:
        break;
      default:
        return;
    }
  } else if (node->IsIgnored()) {
    return;
  }

  HWND hwnd = GetParentHWND();
  if (!hwnd)
    return;

  // Pass the negation of this node's unique id in the |child_id|
  // argument to NotifyWinEvent; the AT client will then call get_accChild
  // on the HWND's accessibility object and pass it that same id, which
  // we can use to retrieve the IAccessible for this node.
  auto* const com = ToBrowserAccessibilityWin(node)->GetCOM();
  TRACE_EVENT("accessibility", "NotifyWinEvent",
              perfetto::Flow::FromPointer(com), "win_event_type",
              base::StringPrintf("0x%04lX", win_event_type));
  ::NotifyWinEvent(win_event_type, hwnd, OBJID_CLIENT, -(com->GetUniqueId()));
}

bool BrowserAccessibilityManagerWin::IsIgnoredChangedNode(
    const BrowserAccessibility* node) const {
  return base::Contains(ignored_changed_nodes_,
                        const_cast<BrowserAccessibility*>(node));
}

void BrowserAccessibilityManagerWin::FireUiaAccessibilityEvent(
    LONG uia_event,
    BrowserAccessibility* node) {
  if (!AXPlatform::GetInstance().IsUiaProviderEnabled()) {
    return;
  }
  if (!ShouldFireEventForNode(node))
    return;

  // Suppress most events when the node just became ignored/unignored.
  if (IsIgnoredChangedNode(node)) {
    switch (uia_event) {
      case UIA_LiveRegionChangedEventId:
        // Don't suppress live region changed events on nodes that just became
        // unignored, but suppress them on nodes that just became ignored. This
        // ensures that ATs can announce LiveRegionChanged events on nodes that
        // just appeared in the tree and not announce the ones that just got
        // removed.
        if (node->IsIgnored())
          return;
        break;
      case UIA_MenuClosedEventId:
      case UIA_MenuOpenedEventId:
        // Don't suppress MenuClosed/MenuOpened events since a change in the
        // ignored state may hide/show a popup by exposing it to the tree or
        // not.
        break;
      default:
        return;
    }
  } else if (node->IsIgnored()) {
    return;
  }

  WinAccessibilityAPIUsageScopedUIAEventsNotifier scoped_events_notifier;

  ::UiaRaiseAutomationEvent(ToBrowserAccessibilityWin(node)->GetCOM(),
                            uia_event);
}

void BrowserAccessibilityManagerWin::FireUiaPropertyChangedEvent(
    LONG uia_property,
    BrowserAccessibility* node) {
  if (!AXPlatform::GetInstance().IsUiaProviderEnabled()) {
    return;
  }
  if (!ShouldFireEventForNode(node))
    return;
  // Suppress events when |IGNORED_CHANGED| with the exception for firing
  // UIA_AriaPropertiesPropertyId-hidden event on non-text node marked as
  // ignored.
  if (node->IsIgnored() || IsIgnoredChangedNode(node)) {
    if (uia_property != UIA_AriaPropertiesPropertyId || node->IsText())
      return;
  }

  // The old value is not used by the system
  VARIANT old_value = {};
  old_value.vt = VT_EMPTY;

  WinAccessibilityAPIUsageScopedUIAEventsNotifier scoped_events_notifier;

  auto* provider = ToBrowserAccessibilityWin(node)->GetCOM();
  base::win::ScopedVariant new_value;
  if (SUCCEEDED(
          provider->GetPropertyValueImpl(uia_property, new_value.Receive()))) {
    ::UiaRaiseAutomationPropertyChangedEvent(provider, uia_property, old_value,
                                             new_value);
  }
}

void BrowserAccessibilityManagerWin::FireUiaStructureChangedEvent(
    StructureChangeType change_type,
    BrowserAccessibility* node) {
  if (!AXPlatform::GetInstance().IsUiaProviderEnabled()) {
    return;
  }
  if (!ShouldFireEventForNode(node))
    return;
  // Suppress events when |IGNORED_CHANGED| except for related structure changes
  if (IsIgnoredChangedNode(node)) {
    switch (change_type) {
      case StructureChangeType_ChildRemoved:
      case StructureChangeType_ChildAdded:
        break;
      default:
        return;
    }
  } else if (node->IsIgnored()) {
    return;
  }

  auto* provider = ToBrowserAccessibilityWin(node);
  auto* provider_com = provider ? provider->GetCOM() : nullptr;
  if (!provider || !provider_com)
    return;

  WinAccessibilityAPIUsageScopedUIAEventsNotifier scoped_events_notifier;

  switch (change_type) {
    case StructureChangeType_ChildRemoved: {
      // 'ChildRemoved' fires on the parent and provides the runtime ID of
      // the removed child (which was passed as |node|).
      auto* parent = ToBrowserAccessibilityWin(node->PlatformGetParent());
      auto* parent_com = parent ? parent->GetCOM() : nullptr;
      if (parent && parent_com) {
        AXPlatformNodeWin::RuntimeIdArray runtime_id;
        provider_com->GetRuntimeIdArray(runtime_id);
        UiaRaiseStructureChangedEvent(parent_com, change_type,
                                      runtime_id.data(), runtime_id.size());
      }
      break;
    }

    default: {
      // All other types are fired on |node|.  For 'ChildAdded' |node| is the
      // child that was added; for other types, it's the parent container.
      UiaRaiseStructureChangedEvent(provider_com, change_type, nullptr, 0);
    }
  }
}

// static
bool BrowserAccessibilityManagerWin::
    IsUiaActiveTextPositionChangedEventSupported() {
  return GetUiaActiveTextPositionChangedEventFunction();
}

// static
UiaRaiseActiveTextPositionChangedEventFunction
BrowserAccessibilityManagerWin::GetUiaActiveTextPositionChangedEventFunction() {
  // MSDN
  // (https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcoreapi/nf-uiautomationcoreapi-uiaraiseactivetextpositionchangedevent)
  // claims this API is fully supported from Win8.1 ownwards, but older
  // versions of this dll on Win10 (e.g., Windows-10-15063 aka. version 1703)
  // don't seem to have the API, which makes chrome.dll fail to load, if
  // ::UiaRaiseActiveTextPositionChangedEvent is called directly. On these older
  // versions of Win10, we will return nullptr.
  return reinterpret_cast<UiaRaiseActiveTextPositionChangedEventFunction>(
      ::GetProcAddress(::GetModuleHandle(L"uiautomationcore.dll"),
                       "UiaRaiseActiveTextPositionChangedEvent"));
}

void BrowserAccessibilityManagerWin::FireUiaActiveTextPositionChangedEvent(
    BrowserAccessibility* node) {
  if (!ShouldFireEventForNode(node))
    return;

  UiaRaiseActiveTextPositionChangedEventFunction
      active_text_position_changed_func =
          GetUiaActiveTextPositionChangedEventFunction();

  if (!active_text_position_changed_func)
    return;

  // Create the text range contained by the target node.
  auto* target_node = ToBrowserAccessibilityWin(node)->GetCOM();
  Microsoft::WRL::ComPtr<ITextRangeProvider> text_range;
  AXPlatformNodeTextProviderWin::CreateDegenerateRangeAtStart(target_node,
                                                              &text_range);

  // Fire the UiaRaiseActiveTextPositionChangedEvent.
  active_text_position_changed_func(target_node, text_range.Get());
}

bool BrowserAccessibilityManagerWin::CanFireEvents() const {
  return BrowserAccessibilityManager::CanFireEvents() &&
         GetDelegateFromRootManager() &&
         GetDelegateFromRootManager()->AccessibilityGetAcceleratedWidget();
}

void BrowserAccessibilityManagerWin::OnSubtreeWillBeDeleted(AXTree* tree,
                                                            AXNode* node) {
  BrowserAccessibility* obj = GetFromAXNode(node);
  DCHECK(obj);
  if (obj) {
    FireWinAccessibilityEvent(EVENT_OBJECT_HIDE, obj);
    FireUiaStructureChangedEvent(StructureChangeType_ChildRemoved, obj);
  }
}

void BrowserAccessibilityManagerWin::OnAtomicUpdateFinished(
    AXTree* tree,
    bool root_changed,
    const std::vector<AXTreeObserver::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(tree, root_changed,
                                                      changes);

  // Do a sequence of Windows-specific updates on each node. Each one is
  // done in a single pass that must complete before the next step starts.
  // The nodes that need to be updated are all of the nodes that were changed,
  // plus some parents.
  std::set<AXPlatformNode*> objs_to_update;
  CollectChangedNodesAndParentsForAtomicUpdate(tree, changes, &objs_to_update);

  // The first step moves win_attributes_ to old_win_attributes_ and then
  // recomputes all of win_attributes_ other than IAccessibleText.
  for (auto* node : objs_to_update) {
    static_cast<BrowserAccessibilityComWin*>(node)
        ->UpdateStep1ComputeWinAttributes();
  }

  // The next step updates the hypertext of each node, which is a
  // concatenation of all of its child text nodes, so it can't run until
  // the text of all of the nodes was computed in the previous step.
  for (auto* node : objs_to_update) {
    static_cast<BrowserAccessibilityComWin*>(node)
        ->UpdateStep2ComputeHypertext();
  }

  // The third step fires events on nodes based on what's changed - like
  // if the name, value, or description changed, or if the hypertext had
  // text inserted or removed. It's able to figure out exactly what changed
  // because we still have old_win_attributes_ populated.
  // This step has to run after the previous two steps complete because the
  // client may walk the tree when it receives any of these events.
  // At the end, it deletes old_win_attributes_ since they're not needed
  // anymore.
  for (auto* node : objs_to_update) {
    static_cast<BrowserAccessibilityComWin*>(node)->UpdateStep3FireEvents();
  }
}

// static
bool BrowserAccessibilityManagerWin::IsIA2NodeSelected(
    BrowserAccessibility* node) {
  return node->IsIA2NodeSelected();
}

// static
bool BrowserAccessibilityManagerWin::IsUIANodeSelected(
    BrowserAccessibility* node) {
  return node->IsUIANodeSelected();
}

void BrowserAccessibilityManagerWin::FireIA2SelectionEvents(
    BrowserAccessibility* container,
    BrowserAccessibility* only_selected_child,
    const SelectionEvents& changes) {
  if (only_selected_child) {
    // Fire 'ElementSelected' on the only selected child.
    FireWinAccessibilityEvent(EVENT_OBJECT_SELECTION, only_selected_child);
  } else {
    const bool container_is_multiselectable =
        container && container->HasState(ax::mojom::State::kMultiselectable);
    for (BrowserAccessibility* item : changes.added) {
      if (container_is_multiselectable)
        FireWinAccessibilityEvent(EVENT_OBJECT_SELECTIONADD, item);
      else
        FireWinAccessibilityEvent(EVENT_OBJECT_SELECTION, item);
    }
    for (BrowserAccessibility* item : changes.removed) {
      FireWinAccessibilityEvent(EVENT_OBJECT_SELECTIONREMOVE, item);
    }
  }
}

void BrowserAccessibilityManagerWin::FireUIASelectionEvents(
    BrowserAccessibility* container,
    BrowserAccessibility* only_selected_child,
    const SelectionEvents& changes) {
  if (only_selected_child) {
    // Fire 'ElementSelected' on the only selected child.
    FireUiaAccessibilityEvent(UIA_SelectionItem_ElementSelectedEventId,
                              only_selected_child);
    FireUiaPropertyChangedEvent(UIA_SelectionItemIsSelectedPropertyId,
                                only_selected_child);
    for (BrowserAccessibility* item : changes.removed) {
      FireUiaPropertyChangedEvent(UIA_SelectionItemIsSelectedPropertyId, item);
    }
  } else {
    // Per UIA documentation, beyond the "invalidate limit" we're supposed to
    // fire a 'SelectionInvalidated' event.  The exact value isn't specified,
    // but System.Windows.Automation.Provider uses a value of 20.
    static const size_t kInvalidateLimit = 20;
    if ((changes.added.size() + changes.removed.size()) > kInvalidateLimit) {
      DCHECK_NE(container, nullptr);
      FireUiaAccessibilityEvent(UIA_Selection_InvalidatedEventId, container);
    } else {
      const bool container_is_multiselectable =
          container && container->HasState(ax::mojom::State::kMultiselectable);
      for (BrowserAccessibility* item : changes.added) {
        if (container_is_multiselectable) {
          FireUiaAccessibilityEvent(
              UIA_SelectionItem_ElementAddedToSelectionEventId, item);
        } else {
          FireUiaAccessibilityEvent(UIA_SelectionItem_ElementSelectedEventId,
                                    item);
        }
        FireUiaPropertyChangedEvent(UIA_SelectionItemIsSelectedPropertyId,
                                    item);
      }
      for (BrowserAccessibility* item : changes.removed) {
        FireUiaAccessibilityEvent(
            UIA_SelectionItem_ElementRemovedFromSelectionEventId, item);
        FireUiaPropertyChangedEvent(UIA_SelectionItemIsSelectedPropertyId,
                                    item);
      }
    }
  }
}

// static
void BrowserAccessibilityManagerWin::HandleSelectedStateChanged(
    SelectionEventsMap& selection_events_map,
    BrowserAccessibility* node,
    bool is_selected) {
  // If |node| belongs to a selection container, then map the events with the
  // selection container as the key because |FinalizeSelectionEvents| needs to
  // determine whether or not there is only one element selected in order to
  // optimize what platform events are sent.
  BrowserAccessibility* key = node;
  if (auto* selection_container = node->PlatformGetSelectionContainer())
    key = selection_container;

  if (is_selected)
    selection_events_map[key].added.push_back(node);
  else
    selection_events_map[key].removed.push_back(node);
}

// static
void BrowserAccessibilityManagerWin::FinalizeSelectionEvents(
    SelectionEventsMap& selection_events_map,
    IsSelectedPredicate is_selected_predicate,
    FirePlatformSelectionEventsCallback fire_platform_events_callback) {
  for (auto&& selected : selection_events_map) {
    BrowserAccessibility* key_node = selected.first;
    SelectionEvents& changes = selected.second;

    // Determine if |node| is a selection container with one selected child in
    // order to optimize what platform events are sent.
    BrowserAccessibility* container = nullptr;
    BrowserAccessibility* only_selected_child = nullptr;
    if (IsContainerWithSelectableChildren(key_node->GetRole())) {
      container = key_node;
      for (auto it = container->InternalChildrenBegin();
           it != container->InternalChildrenEnd(); ++it) {
        auto* child = it.get();
        if (is_selected_predicate.Run(child)) {
          if (!only_selected_child) {
            only_selected_child = child;
            continue;
          }

          only_selected_child = nullptr;
          break;
        }
      }
    }

    fire_platform_events_callback.Run(container, only_selected_child, changes);
  }

  selection_events_map.clear();
}

void BrowserAccessibilityManagerWin::HandleAriaPropertiesChangedEvent(
    BrowserAccessibility& node) {
  DCHECK_IN_ON_ACCESSIBILITY_EVENTS();
  aria_properties_events_.insert(&node);
}

void BrowserAccessibilityManagerWin::EnqueueTextChangedEvent(
    BrowserAccessibility& node) {
  DCHECK_IN_ON_ACCESSIBILITY_EVENTS();
  if (BrowserAccessibility* text_provider = GetUiaTextPatternProvider(node))
    text_changed_nodes_.insert(text_provider);
}

void BrowserAccessibilityManagerWin::EnqueueSelectionChangedEvent(
    BrowserAccessibility& node) {
  DCHECK_IN_ON_ACCESSIBILITY_EVENTS();
  selection_changed_nodes_.insert(&node);
}

gfx::Rect BrowserAccessibilityManagerWin::GetViewBoundsInScreenCoordinates()
    const {
  AXPlatformTreeManagerDelegate* delegate = GetDelegateFromRootManager();
  if (!delegate) {
    return gfx::Rect();
  }

  gfx::Rect bounds = delegate->AccessibilityGetViewBounds();

  // On Windows, we cannot directly multiply the bounds in screen DIPs by the
  // display's scale factor to get screen physical coordinates like we can on
  // other platforms. We need to go through the ScreenWin::DIPToScreenRect
  // helper function to perform the right set of offset transformations needed.
  //
  // This is because Chromium transforms the screen physical coordinates it
  // receives from Windows into an internal representation of screen physical
  // coordinates adjusted for multiple displays of different resolutions.
  return display::win::ScreenWin::DIPToScreenRect(GetParentHWND(), bounds);
}

void BrowserAccessibilityManagerWin::BeforeAccessibilityEvents() {
  BrowserAccessibilityManager::BeforeAccessibilityEvents();

  DCHECK(aria_properties_events_.empty());
  DCHECK(text_changed_nodes_.empty());
  DCHECK(selection_changed_nodes_.empty());
  DCHECK(ignored_changed_nodes_.empty());

  for (const auto& targeted_event : event_generator()) {
    if (targeted_event.event_params->event ==
        AXEventGenerator::Event::IGNORED_CHANGED) {
      BrowserAccessibility* event_target = GetFromID(targeted_event.node_id);
      if (!event_target)
        continue;

      const auto insert_pair = ignored_changed_nodes_.insert(event_target);

      // Expect that |IGNORED_CHANGED| only fires once for a given
      // node in a given event frame.
      DCHECK(insert_pair.second);
    }
  }
}

void BrowserAccessibilityManagerWin::FinalizeAccessibilityEvents() {
  BrowserAccessibilityManager::FinalizeAccessibilityEvents();

  // Finalize aria properties events.
  for (BrowserAccessibility* event_node : aria_properties_events_)
    FireUiaPropertyChangedEvent(UIA_AriaPropertiesPropertyId, event_node);
  aria_properties_events_.clear();

  // Finalize selection changed events.
  for (BrowserAccessibility* event_node : selection_changed_nodes_) {
    DCHECK(event_node);
    if (ToBrowserAccessibilityWin(event_node)
            ->GetCOM()
            ->IsPatternProviderSupported(UIA_TextPatternId)) {
      FireUiaAccessibilityEvent(UIA_Text_TextSelectionChangedEventId,
                                event_node);
    }
  }
  selection_changed_nodes_.clear();

  // Finalize text changed events.
  for (BrowserAccessibility* event_node : text_changed_nodes_)
    FireUiaAccessibilityEvent(UIA_Text_TextChangedEventId, event_node);
  text_changed_nodes_.clear();

  // Finalize selection item events.
  FinalizeSelectionEvents(
      ia2_selection_events_, base::BindRepeating(&IsIA2NodeSelected),
      base::BindRepeating(
          &BrowserAccessibilityManagerWin::FireIA2SelectionEvents,
          base::Unretained(this)));
  FinalizeSelectionEvents(
      uia_selection_events_, base::BindRepeating(&IsUIANodeSelected),
      base::BindRepeating(
          &BrowserAccessibilityManagerWin::FireUIASelectionEvents,
          base::Unretained(this)));

  ignored_changed_nodes_.clear();
}

BrowserAccessibilityManagerWin::SelectionEvents::SelectionEvents() = default;
BrowserAccessibilityManagerWin::SelectionEvents::~SelectionEvents() = default;

}  // namespace ui
