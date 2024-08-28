// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_TREE_MANAGER_H_
#define UI_ACCESSIBILITY_AX_TREE_MANAGER_H_

#include "base/scoped_observation.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_observer.h"

namespace ui {

class AXNode;
class AXTreeManagerMap;

// Interface for a class that owns an AXTree and manages its connections
// to other AXTrees in the same page or desktop (parent and child trees)
// as well as a mapping of AXNode's by ID for supporting `GetNodeFromTree`
// and related methods.
//
// Note, the tree manager may be created for a tree which has unknown (not
// valid) tree id. A such tree is not registered with the tree map and thus
// cannot be retrieved from the map. When the tree gets data and tree id, then
// it is registered in the map automatically (see OnTreeDataChanged callback
// notification). The mechanism implements the tree id data integrirty between
// the tree map and trees, also it doesn't allow to register two different trees
// with unknown IDs.
class AX_EXPORT AXTreeManager : public AXTreeObserver {
 public:
  static AXTreeManager* FromID(const AXTreeID& ax_tree_id);
  // If the child of `parent_node` exists in a separate child tree, return the
  // tree manager for that child tree. Otherwise, return nullptr.
  static AXTreeManager* ForChildTree(const AXNode& parent_node);

  // For testing only, get the registered focus change callback
  static base::RepeatingClosure& GetFocusChangeCallbackForTesting();
  // For testing only, register a function to be called when focus changes
  // in any AXTreeManager.
  static void SetFocusChangeCallbackForTesting(base::RepeatingClosure callback);

  // Ensure that any accessibility fatal error crashes the renderer. Once this
  // is turned on, it stays on all renderers, because at this point it is
  // assumed that the user is a developer.
  static void AlwaysFailFast() { is_fail_fast_mode_ = true; }
  static bool IsFailFastMode() { return is_fail_fast_mode_; }

  // This default constructor does not create an empty accessibility tree. Call
  // `SetTree` if you need to manage a specific tree.
  AXTreeManager();
  explicit AXTreeManager(std::unique_ptr<AXTree> tree);

  AXTreeManager(const AXTreeManager&) = delete;
  AXTreeManager& operator=(const AXTreeManager&) = delete;

  ~AXTreeManager() override;

  enum class RetargetEventType {
    RetargetEventTypeGenerated = 0,
    RetargetEventTypeBlinkGeneral,
    RetargetEventTypeBlinkHover,
  };

  // Subclasses override these methods to send native event notifications.
  virtual void FireFocusEvent(AXNode* node);
  // Return |node| by default, but some platforms want to update the target node
  // based on the event type.
  virtual AXNode* RetargetForEvents(AXNode* node, RetargetEventType type) const;
  virtual void FireGeneratedEvent(AXEventGenerator::Event event_type,
                                  const AXNode* node) {}
  virtual bool CanFireEvents() const;

  // Returns whether or not this tree manager is for a view.
  virtual bool IsView() const;

  // Returns the AXNode with the given |node_id| from the tree that has the
  // given |tree_id|. This allows for callers to access nodes outside of their
  // own tree. Returns nullptr if |tree_id| or |node_id| is not found.
  // TODO(kschmi): Remove |tree_id| parameter, as it's unnecessary.
  virtual AXNode* GetNodeFromTree(const AXTreeID& tree_id,
                                  const AXNodeID node_id) const;

  // Returns the AXNode in the current tree that has the given |node_id|.
  // Returns nullptr if |node_id| is not found.
  virtual AXNode* GetNode(const AXNodeID node_id) const;

  // Returns true if the manager has a tree with a valid (not unknown) ID.
  bool HasValidTreeID() const {
    return ax_tree_ && ax_tree_->GetAXTreeID() != AXTreeIDUnknown();
  }

  // Returns the tree id of the tree managed by this AXTreeManager.
  const AXTreeID& GetTreeID() const {
    return ax_tree_ ? ax_tree_->GetAXTreeID() : AXTreeIDUnknown();
  }

  // Returns the AXTreeData for the tree managed by this AXTreeManager.
  const AXTreeData& GetTreeData() const;

  // Returns the tree id of the parent tree.
  // Returns AXTreeIDUnknown if this tree doesn't have a parent tree.
  virtual AXTreeID GetParentTreeID() const;

  // Whether this manager can access platform nodes. Defaults to false
  // and is overridden in `AXPlatformTreeManager` to return true.
  virtual bool IsPlatformTreeManager() const;

  // Returns the AXNode that is at the root of the current tree.
  virtual AXNode* GetRoot() const;

  bool IsRoot() const;

  // Returns the root AXTreeManager by walking up the tree to any parent trees.
  // If there is a parent tree that is not yet connected, returns nullptr.
  AXTreeManager* GetRootManager() const;

  // If this tree has a parent tree, returns the node in the parent tree that
  // hosts the current tree. Returns nullptr if this tree doesn't have a parent
  // tree.
  virtual AXNode* GetParentNodeFromParentTree() const;

  void Initialize(const AXTreeUpdate& initial_tree);

  // Called when the tree manager is about to be removed from the tree map,
  // `AXTreeManagerMap`.
  void WillBeRemovedFromMap();

  // Returns a pointer to the managed tree, if any.
  AXTree* ax_tree() const { return ax_tree_.get(); }

  // Takes ownership of a new accessibility tree and returns the one that is
  // currently being managed. It is considered an error to pass an empty
  // unique_ptr for `tree`. If no tree is currently being managed, returns an
  // empty unique_ptr.
  std::unique_ptr<AXTree> SetTree(std::unique_ptr<AXTree> tree);
  std::unique_ptr<AXTree> SetTree(const AXTreeUpdate& initial_state);

  const AXEventGenerator& event_generator() const { return event_generator_; }
  AXEventGenerator& event_generator() { return event_generator_; }

  // AXTreeObserver implementation.
  void OnTreeDataChanged(AXTree* tree,
                         const AXTreeData& old_data,
                         const AXTreeData& new_data) override;
  void OnNodeWillBeDeleted(AXTree* tree, AXNode* node) override;
  void OnSubtreeWillBeDeleted(AXTree* tree, AXNode* node) override {}
  void OnNodeCreated(AXTree* tree, AXNode* node) override {}
  void OnNodeDeleted(AXTree* tree, int32_t node_id) override {}
  void OnNodeReparented(AXTree* tree, AXNode* node) override {}
  void OnRoleChanged(AXTree* tree,
                     AXNode* node,
                     ax::mojom::Role old_role,
                     ax::mojom::Role new_role) override {}
  void OnAtomicUpdateFinished(
      AXTree* tree,
      bool root_changed,
      const std::vector<AXTreeObserver::Change>& changes) override;

 protected:
  // This is only made protected to accommodate the `AtomicViewAXTreeManager`.
  // It should be made private once that class is removed.
  // TODO(crbug.com/40924888): Make private.
  static AXTreeManagerMap& GetMap();

  virtual AXTreeManager* GetParentManager() const;

  // Return the last node that had focus, no searching.
  static AXNode* GetLastFocusedNode();

  static void SetLastFocusedNode(AXNode* node);

  // Add parent connection if missing (!connected_to_parent_tree_node_). If the
  // root's parent is in another accessibility tree but it wasn't previously
  // connected, post the proper notifications on the parent.
  void EnsureParentConnectionIfNotRootManager();

  // Refreshes a parent node in a parent tree when it needs to be informed that
  // this tree is ready or being destroyed.
  void ParentConnectionChanged(AXNode* parent);

  // Inheriting classes should override this method to update the `parent`
  // attributes accordingly when the parent connection changes.
  virtual void UpdateAttributesOnParent(AXNode* parent) {}

  // True if the root's parent is in another accessibility tree and that
  // parent's child is the root. Ensures that the parent node is notified
  // once when this subtree is first connected.
  bool connected_to_parent_tree_node_;

  std::unique_ptr<AXTree> ax_tree_;

  AXEventGenerator event_generator_;

  // Stores the id of the last focused node, as well as the id
  // of the tree that contains it, so that when focus might have changed we can
  // figure out whether we need to fire a focus event.
  //
  // NOTE: Don't use or modify these properties directly, use the
  // SetLastFocusedNode and GetLastFocusedNode methods instead.
  static std::optional<AXNodeID> last_focused_node_id_;
  static std::optional<AXTreeID> last_focused_node_tree_id_;

 private:
  friend class TestSingleAXTreeManager;

  // A flag to ensure that accessibility fatal errors crash immediately.
  static bool is_fail_fast_mode_;

  // Automatically stops observing notifications from the AXTree when this class
  // is destructed.
  //
  // This member needs to be destructed before any observed AXTrees. Since
  // destructors for non-static member fields are called in the reverse order of
  // declaration, do not move this member above other members.
  base::ScopedObservation<AXTree, AXTreeObserver> tree_observation_{this};
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_TREE_MANAGER_H_
