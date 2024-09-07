// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_

#include <oleacc.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/display/win/screen_win.h"

namespace ui {
class AXPlatformTreeManagerDelegate;
}

namespace ui {

class BrowserAccessibilityWin;

using UiaRaiseActiveTextPositionChangedEventFunction =
    HRESULT(WINAPI*)(IRawElementProviderSimple*, ITextRangeProvider*);

// Manages a tree of BrowserAccessibilityWin objects.
class COMPONENT_EXPORT(AX_PLATFORM) BrowserAccessibilityManagerWin
    : public BrowserAccessibilityManager {
 public:
  BrowserAccessibilityManagerWin(const AXTreeUpdate& initial_tree,
                                 AXNodeIdDelegate& node_id_delegate,
                                 AXPlatformTreeManagerDelegate* delegate);

  BrowserAccessibilityManagerWin(const BrowserAccessibilityManagerWin&) =
      delete;
  BrowserAccessibilityManagerWin& operator=(
      const BrowserAccessibilityManagerWin&) = delete;

  ~BrowserAccessibilityManagerWin() override;

  static AXTreeUpdate GetEmptyDocument();
  static bool IsUiaActiveTextPositionChangedEventSupported();

  // Get the closest containing HWND.
  HWND GetParentHWND() const;

  // BrowserAccessibilityManager methods
  void UserIsReloading() override;
  bool IsIgnoredChangedNode(const BrowserAccessibility* node) const;
  bool CanFireEvents() const override;

  void FireAriaNotificationEvent(
      BrowserAccessibility* node,
      const std::string& announcement,
      const std::string& notification_id,
      ax::mojom::AriaNotificationInterrupt interrupt_property,
      ax::mojom::AriaNotificationPriority priority_property) override;

  void FireFocusEvent(AXNode* node) override;
  void FireBlinkEvent(ax::mojom::Event event_type,
                      BrowserAccessibility* node,
                      int action_request_id) override;
  void FireGeneratedEvent(AXEventGenerator::Event event_type,
                          const AXNode* node) override;

  void FireWinAccessibilityEvent(LONG win_event, BrowserAccessibility* node);
  void FireUiaAccessibilityEvent(LONG uia_event, BrowserAccessibility* node);
  void FireUiaActiveTextPositionChangedEvent(BrowserAccessibility* node);
  void FireUiaPropertyChangedEvent(LONG uia_property,
                                   BrowserAccessibility* node);
  void FireUiaStructureChangedEvent(StructureChangeType change_type,
                                    BrowserAccessibility* node);

  gfx::Rect GetViewBoundsInScreenCoordinates() const override;

  // Do event pre-processing
  void BeforeAccessibilityEvents() override;

  // Do event post-processing
  void FinalizeAccessibilityEvents() override;

 protected:
  // AXTreeObserver methods.
  void OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) override;
  void OnAtomicUpdateFinished(
      AXTree* tree,
      bool root_changed,
      const std::vector<AXTreeObserver::Change>& changes) override;

 private:
  struct SelectionEvents {
    std::vector<raw_ptr<BrowserAccessibility, VectorExperimental>> added;
    std::vector<raw_ptr<BrowserAccessibility, VectorExperimental>> removed;
    SelectionEvents();
    ~SelectionEvents();
  };

  using SelectionEventsMap = std::map<BrowserAccessibility*, SelectionEvents>;
  using IsSelectedPredicate =
      base::RepeatingCallback<bool(BrowserAccessibility*)>;
  using FirePlatformSelectionEventsCallback =
      base::RepeatingCallback<void(BrowserAccessibility*,
                                   BrowserAccessibility*,
                                   const SelectionEvents&)>;
  static bool IsIA2NodeSelected(BrowserAccessibility* node);
  static bool IsUIANodeSelected(BrowserAccessibility* node);

  void FireIA2SelectionEvents(BrowserAccessibility* container,
                              BrowserAccessibility* only_selected_child,
                              const SelectionEvents& changes);
  void FireUIASelectionEvents(BrowserAccessibility* container,
                              BrowserAccessibility* only_selected_child,
                              const SelectionEvents& changes);

  static void HandleSelectedStateChanged(
      SelectionEventsMap& selection_events_map,
      BrowserAccessibility* node,
      bool is_selected);

  static void FinalizeSelectionEvents(
      SelectionEventsMap& selection_events_map,
      IsSelectedPredicate is_selected_predicate,
      FirePlatformSelectionEventsCallback fire_platform_events_callback);

  // Retrieve UIA RaiseActiveTextPositionChangedEvent function if supported.
  static UiaRaiseActiveTextPositionChangedEventFunction
  GetUiaActiveTextPositionChangedEventFunction();

  void HandleAriaPropertiesChangedEvent(BrowserAccessibility& node);
  void EnqueueTextChangedEvent(BrowserAccessibility& node);
  void EnqueueSelectionChangedEvent(BrowserAccessibility& node);

  // Give BrowserAccessibilityManager::Create access to our constructor.
  friend class BrowserAccessibilityManager;

  // Since there could be multiple aria property changes on a node and we only
  // want to fire UIA_AriaPropertiesPropertyId once for that node, we use the
  // set here to keep track of the unique nodes that had aria property changes,
  // so we only fire the event once for every node.
  std::set<raw_ptr<BrowserAccessibility, SetExperimental>>
      aria_properties_events_;

  // Since there could be duplicate selection changed events on a node raised
  // from both EventType::DOCUMENT_SELECTION_CHANGED and
  // EventType::TEXT_SELECTION_CHANGED, we keep track of the unique
  // nodes so we only fire the event once for every node.
  std::set<raw_ptr<BrowserAccessibility, SetExperimental>>
      selection_changed_nodes_;

  // Since there could be duplicate text changed events on a node raised from
  // both FireBlinkEvent and FireGeneratedEvent, we use the set here to keep
  // track of the unique nodes that had UIA_Text_TextChangedEventId, so we only
  // fire the event once for every node.
  std::set<raw_ptr<BrowserAccessibility, SetExperimental>> text_changed_nodes_;

  // When the ignored state changes for a node, we only want to fire the
  // events relevant to the ignored state change (e.g. show / hide).
  // This set keeps track of what nodes should suppress superfluous events.
  std::set<raw_ptr<BrowserAccessibility, SetExperimental>>
      ignored_changed_nodes_;

  // Keep track of selection changes so we can optimize UIA event firing.
  // Pointers are only stored for the duration of |OnAccessibilityEvents|, and
  // the map is cleared in |FinalizeAccessibilityEvents|.
  SelectionEventsMap ia2_selection_events_;
  SelectionEventsMap uia_selection_events_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_MANAGER_WIN_H_
