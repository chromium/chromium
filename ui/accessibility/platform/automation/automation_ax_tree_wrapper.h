// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_AX_TREE_WRAPPER_H_
#define UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_AX_TREE_WRAPPER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_update.h"

namespace ui {

class AutomationTreeManagerOwner;

// A class that wraps one AXTree and all of the additional state
// and helper methods needed to use it for the automation API.
class COMPONENT_EXPORT(AX_PLATFORM) AutomationAXTreeWrapper
    : public AXTreeManager {
 public:
  explicit AutomationAXTreeWrapper(AutomationTreeManagerOwner* owner);

  AutomationAXTreeWrapper(const AutomationAXTreeWrapper&) = delete;
  AutomationAXTreeWrapper& operator=(const AutomationAXTreeWrapper&) = delete;

  ~AutomationAXTreeWrapper() override;

  // Returns the AutomationAXTreeWrapper that lists |tree_id| as one of its
  // child trees, if any.
  static AutomationAXTreeWrapper* GetParentOfTreeId(AXTreeID tree_id);

  static std::map<AXTreeID, AutomationAXTreeWrapper*>&
  GetChildTreeIDReverseMap();

  // Multiroot tree lookup.
  static AXNode* GetParentTreeNodeForAppID(
      const std::string& app_id,
      const AutomationTreeManagerOwner* owner);
  static AutomationAXTreeWrapper* GetParentTreeWrapperForAppID(
      const std::string& app_id,
      const AutomationTreeManagerOwner* owner);
  static std::vector<AXNode*> GetChildTreeNodesForAppID(
      const std::string& app_id,
      const AutomationTreeManagerOwner* owner);

  AutomationTreeManagerOwner* owner() { return owner_; }

  // Called by AutomationInternalCustomBindings::DispatchAccessibilityEvents on
  // the AutomationAXTreeWrapper instance for the correct tree corresponding
  // to this event. Unserializes the tree update and calls back to
  // AutomationTreeManagerOwner to fire any automation events needed.
  bool OnAccessibilityEvents(const AXTreeID& tree_id,
                             const std::vector<AXTreeUpdate>& updates,
                             const std::vector<AXEvent>& events,
                             gfx::Point mouse_location);

  // Returns true if this is the desktop tree.
  bool IsDesktopTree() const;

  // Returns whether this tree is scaled by a device scale factor.
  bool HasDeviceScaleFactor() const;

  // Returns whether |node_id| is the focused node in this tree. Accounts for
  // cases where this tree itself is not focused. Behaves similarly to
  // document.activeElement (within the DOM).
  bool IsInFocusChain(int32_t node_id);

  AXSelection GetUnignoredSelection();

  // Returns an AXNode from the underlying tree if it both exists and is not
  // ignored.
  AXNode* GetUnignoredNodeFromId(int32_t id);

  // Accessibility focus is globally set via automation API from js.
  void SetAccessibilityFocus(int32_t node_id);
  AXNode* GetAccessibilityFocusedNode();

  int32_t accessibility_focused_id() { return accessibility_focused_id_; }

  // Gets the parent tree wrapper.
  AutomationAXTreeWrapper* GetParentTree();

  // Gets the first tree wrapper with an unignored root. This can be |this| tree
  // wrapper or an ancestor. A root is ignored if the tree has valid nodes with
  // a valid ax::mojom::StringAttribute::kChildTreeNodeAppId making the tree
  // have multiple roots.
  AutomationAXTreeWrapper* GetTreeWrapperWithUnignoredRoot();

  // Gets a parent tree wrapper by following the first valid parent tree node
  // app id. Useful if this tree contains app ids that always reference the same
  // parent tree.
  AutomationAXTreeWrapper* GetParentTreeFromAnyAppID();

  // Updates or gets this wrapper with the latest state of listeners in js.
  void EventListenerAdded(
      const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type,
      AXNode* node);
  void EventListenerRemoved(
      const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type,
      AXNode* node);
  bool HasEventListener(
      const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type,
      AXNode* node);
  size_t EventListenerCount() const;

  // Indicates whether this tree is ignored due to a hosting ancestor tree/node
  // being ignored.
  bool IsTreeIgnored();

  // AXTreeManager overrides.
  AXNode* GetNodeFromTree(const AXTreeID& tree_id,
                          const AXNodeID node_id) const override;
  AXTreeID GetParentTreeID() const override;
  AXNode* GetParentNodeFromParentTree() const override;

 private:
  // AXTreeObserver overrides.
  void OnNodeDataChanged(AXTree* tree,
                         const AXNodeData& old_node_data,
                         const AXNodeData& new_node_data) override;
  void OnStringAttributeChanged(AXTree* tree,
                                AXNode* node,
                                ax::mojom::StringAttribute attr,
                                const std::string& old_value,
                                const std::string& new_value) override;
  void OnNodeWillBeDeleted(AXTree* tree, AXNode* node) override;
  void OnNodeCreated(AXTree* tree, AXNode* node) override;
  void OnAtomicUpdateFinished(AXTree* tree,
                              bool root_changed,
                              const std::vector<Change>& changes) override;
  void OnIgnoredChanged(AXTree* tree,
                        AXNode* node,
                        bool is_ignored_new_value) override;

  raw_ptr<AutomationTreeManagerOwner> owner_;
  std::vector<int> deleted_node_ids_;
  std::vector<int> text_changed_node_ids_;

  int32_t accessibility_focused_id_ = kInvalidAXNodeID;

  // Tracks whether a tree change event was sent during unserialization. Tree
  // changes outside of unserialization do not get reflected here. The value is
  // reset after unserialization.
  bool did_send_tree_change_during_unserialization_ = false;

  // Maps a node to a set containing events for which the node has listeners.
  std::map<int32_t,
           std::set<std::tuple<ax::mojom::Event, AXEventGenerator::Event>>>
      node_id_to_events_;

  // A collection of all nodes with
  // ax::mojom::StringAttribute::kAppId set.
  std::set<std::string> all_tree_node_app_ids_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AUTOMATION_AUTOMATION_AX_TREE_WRAPPER_H_
