// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_manager.h"

#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager_map.h"
#include "ui/accessibility/ax_tree_observer.h"

namespace ui {

namespace {
// A function to call when focus changes, for testing only.
base::LazyInstance<base::RepeatingClosure>::DestructorAtExit
    g_focus_change_callback_for_testing = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
AXTreeManagerMap& AXTreeManager::GetMap() {
  static base::NoDestructor<AXTreeManagerMap> map;
  return *map;
}

// static
AXTreeManager* AXTreeManager::FromID(const AXTreeID& ax_tree_id) {
  return ax_tree_id != AXTreeIDUnknown() ? GetMap().GetManager(ax_tree_id)
                                         : nullptr;
}

// static
AXTreeManager* AXTreeManager::ForChildTree(const AXNode& parent_node) {
  if (!parent_node.HasStringAttribute(
          ax::mojom::StringAttribute::kChildTreeId)) {
    return nullptr;
  }

  AXTreeID child_tree_id = AXTreeID::FromString(
      parent_node.GetStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
  AXTreeManager* child_tree_manager = GetMap().GetManager(child_tree_id);

  // Some platforms do not use AXTreeManagers, so child trees don't exist in
  // the browser process.
  DCHECK(!child_tree_manager ||
         !child_tree_manager->GetParentNodeFromParentTree() ||
         child_tree_manager->GetParentNodeFromParentTree()->id() ==
             parent_node.id());
  return child_tree_manager;
}

// static
void AXTreeManager::SetFocusChangeCallbackForTesting(
    base::RepeatingClosure callback) {
  g_focus_change_callback_for_testing.Get() = std::move(callback);
}

AXTreeManager::AXTreeManager()
    : connected_to_parent_tree_node_(false),
      ax_tree_id_(AXTreeIDUnknown()),
      ax_tree_(nullptr),
      event_generator_(ax_tree()) {}

AXTreeManager::AXTreeManager(std::unique_ptr<AXTree> tree)
    : connected_to_parent_tree_node_(false),
      ax_tree_id_(tree ? tree->data().tree_id : AXTreeIDUnknown()),
      ax_tree_(std::move(tree)),
      event_generator_(ax_tree()) {
  DCHECK(ax_tree_);
  GetMap().AddTreeManager(ax_tree_id_, this);
  tree_observation_.Observe(ax_tree());
}

AXTreeManager::AXTreeManager(const AXTreeID& tree_id,
                             std::unique_ptr<AXTree> tree)
    : connected_to_parent_tree_node_(false),
      ax_tree_id_(tree_id),
      ax_tree_(std::move(tree)),
      event_generator_(ax_tree()) {
  DCHECK(ax_tree_);
  GetMap().AddTreeManager(ax_tree_id_, this);
  tree_observation_.Observe(ax_tree());
}

void AXTreeManager::FireFocusEvent(AXNode* node) {
  if (g_focus_change_callback_for_testing.Get())
    g_focus_change_callback_for_testing.Get().Run();
}

AXNode* AXTreeManager::RetargetForEvents(AXNode* node,
                                         RetargetEventType type) const {
  return node;
}

bool AXTreeManager::CanFireEvents() const {
  // Delay events until it makes sense to fire them.
  // Events that are generated while waiting until CanFireEvents() returns true
  // are dropped by design. Any events after the page is ready for events will
  // be relative to that initial tree.

  // The current tree must have an AXTreeID.
  if (GetTreeID() == AXTreeIDUnknown())
    return false;

  // Fire events only when the root of the tree is reachable.
  AXTreeManager* root_manager = GetRootManager();
  if (!root_manager)
    return false;

  // Make sure that nodes can be traversed to the root.
  const AXTreeManager* ancestor_manager = this;
  while (!ancestor_manager->IsRoot()) {
    AXNode* host_node = ancestor_manager->GetParentNodeFromParentTree();
    if (!host_node)
      return false;  // Host node not ready yet.
    ancestor_manager = host_node->GetManager();
  }

  return true;
}

AXNode* AXTreeManager::GetNodeFromTree(const AXTreeID& tree_id,
                                       const AXNodeID node_id) const {
  auto* manager = AXTreeManager::FromID(tree_id);
  return manager ? manager->GetNode(node_id) : nullptr;
}

void AXTreeManager::Initialize(const ui::AXTreeUpdate& initial_tree) {
  if (!ax_tree()->Unserialize(initial_tree)) {
    LOG(FATAL) << "No recovery is possible if the initial tree is broken: "
               << ax_tree()->error();
  }
}

AXNode* AXTreeManager::GetNode(const AXNodeID node_id) const {
  return ax_tree_ ? ax_tree_->GetFromId(node_id) : nullptr;
}

AXTreeID AXTreeManager::GetTreeID() const {
  return ax_tree_ ? ax_tree_->data().tree_id : AXTreeIDUnknown();
}

const AXTreeData& AXTreeManager::GetTreeData() const {
  return ax_tree_ ? ax_tree_->data() : AXTreeDataUnknown();
}

AXTreeID AXTreeManager::GetParentTreeID() const {
  return ax_tree_ ? ax_tree_->data().parent_tree_id : AXTreeIDUnknown();
}

AXNode* AXTreeManager::GetRoot() const {
  return ax_tree_ ? ax_tree_->root() : nullptr;
}

void AXTreeManager::WillBeRemovedFromMap() {
  if (!ax_tree_)
    return;
  ax_tree_->NotifyTreeManagerWillBeRemoved(ax_tree_id_);
}

// static
absl::optional<AXNodeID> AXTreeManager::last_focused_node_id_ = {};

// static
absl::optional<AXTreeID> AXTreeManager::last_focused_node_tree_id_ = {};

// static
void AXTreeManager::SetLastFocusedNode(AXNode* node) {
  if (node) {
    DCHECK(node->GetManager());
    last_focused_node_id_ = node->id();
    last_focused_node_tree_id_ = node->GetManager()->GetTreeID();
    DCHECK(last_focused_node_tree_id_);
    DCHECK(last_focused_node_tree_id_ != ui::AXTreeIDUnknown());
  } else {
    last_focused_node_id_.reset();
    last_focused_node_tree_id_.reset();
  }
}

// static
AXNode* AXTreeManager::GetLastFocusedNode() {
  if (last_focused_node_id_) {
    DCHECK(last_focused_node_tree_id_);
    DCHECK(last_focused_node_tree_id_ != ui::AXTreeIDUnknown());
    if (AXTreeManager* last_focused_manager =
            FromID(last_focused_node_tree_id_.value())) {
      return last_focused_manager->GetNode(last_focused_node_id_.value());
    }
  }
  return nullptr;
}

AXTreeManager::~AXTreeManager() {
  AXNode* parent = nullptr;
  if (connected_to_parent_tree_node_)
    parent = GetParentNodeFromParentTree();

  // Fire any events that need to be fired when tree nodes get deleted. For
  // example, events that fire every time "OnSubtreeWillBeDeleted" is called.
  if (ax_tree_)
    ax_tree_->Destroy();

  CleanUp();

  // Stop observing so we don't get a callback for every node being deleted.
  event_generator_.ReleaseTree();
  if (ax_tree_)
    GetMap().RemoveTreeManager(ax_tree_id_);
  if (last_focused_node_tree_id_ && ax_tree_id_ == *last_focused_node_tree_id_)
    SetLastFocusedNode(nullptr);

  ParentConnectionChanged(parent);
}

void AXTreeManager::OnTreeDataChanged(AXTree* tree,
                                      const AXTreeData& old_data,
                                      const AXTreeData& new_data) {
  DCHECK_EQ(ax_tree(), tree);
  if (new_data.tree_id == ui::AXTreeIDUnknown() ||
      new_data.tree_id == ax_tree_id_) {
    // Tree ID hasn't changed.
    return;
  }

  // Either the tree that is being managed by this manager has just been
  // created, or it has been destroyed and re-created.
  connected_to_parent_tree_node_ = false;

  // If the current focus is in the tree that has just been destroyed, then
  // reset the focus to nullptr. It will be set to the current focus again the
  // next time there is a focus event.
  if (ax_tree_id_ != ui::AXTreeIDUnknown() &&
      ax_tree_id_ == last_focused_node_tree_id_) {
    SetLastFocusedNode(nullptr);
  }

  GetMap().RemoveTreeManager(ax_tree_id_);
  ax_tree_id_ = new_data.tree_id;
  GetMap().AddTreeManager(ax_tree_id_, this);
}

void AXTreeManager::OnNodeWillBeDeleted(AXTree* tree, AXNode* node) {
  DCHECK(node);
  if (node == GetLastFocusedNode())
    SetLastFocusedNode(nullptr);

  // We fire these here, immediately, to ensure we can send platform
  // notifications prior to the actual destruction of the object.
  if (node->GetRole() == ax::mojom::Role::kMenu)
    FireGeneratedEvent(AXEventGenerator::Event::MENU_POPUP_END, node);
}

void AXTreeManager::OnAtomicUpdateFinished(
    AXTree* tree,
    bool root_changed,
    const std::vector<AXTreeObserver::Change>& changes) {
  DCHECK_EQ(ax_tree(), tree);
  if (root_changed)
    connected_to_parent_tree_node_ = false;
}

AXTreeManager* AXTreeManager::GetParentManager() const {
  AXTreeID parent_tree_id = GetParentTreeID();
  if (parent_tree_id == ui::AXTreeIDUnknown())
    return nullptr;

  // There's no guarantee that we'll find an AXTreeManager for this AXTreeID, so
  // we might still return nullptr.
  // See `BrowserAccessibilityManager::GetParentManager` for more details.
  return FromID(parent_tree_id);
}

bool AXTreeManager::IsRoot() const {
  return GetParentTreeID() == ui::AXTreeIDUnknown();
}

AXTreeManager* AXTreeManager::GetRootManager() const {
  if (IsRoot())
    return const_cast<AXTreeManager*>(this);

  AXTreeManager* parent = GetParentManager();
  if (!parent) {
    // This can occur when the parent tree is not yet serialized. We can't
    // prevent a child tree from serializing before the parent tree, so we just
    // have to handle this case. Attempting to change this to a DCHECK() will
    // cause a number of tests to fail.
    return nullptr;
  }
  return parent->GetRootManager();
}

AXNode* AXTreeManager::GetParentNodeFromParentTree() const {
  AXTreeManager* parent_manager = GetParentManager();
  if (!parent_manager)
    return nullptr;

  DCHECK(GetRoot());

  std::set<int32_t> host_node_ids =
      parent_manager->ax_tree()->GetNodeIdsForChildTreeId(GetTreeID());
  if (host_node_ids.empty()) {
    // Parent tree has host node but the change has not yet been serialized.
    return nullptr;
  }

  CHECK_EQ(host_node_ids.size(), 1U)
  << "Multiple nodes cannot claim the same child tree ID.";

  AXNode* parent_node = parent_manager->GetNode(*(host_node_ids.begin()));
  DCHECK(parent_node);
  DCHECK_EQ(ax_tree_id_, AXTreeID::FromString(parent_node->GetStringAttribute(
                             ax::mojom::StringAttribute::kChildTreeId)))
      << "A node that hosts a child tree should expose its tree ID in its "
         "`kChildTreeId` attribute.";

  return parent_node;
}

void AXTreeManager::ParentConnectionChanged(AXNode* parent) {
  if (!parent) {
    connected_to_parent_tree_node_ = false;
    return;
  }
  connected_to_parent_tree_node_ = true;

  UpdateAttributesOnParent(parent);
  AXTreeManager* parent_manager = parent->GetManager();
  parent = parent_manager->RetargetForEvents(
      parent, RetargetEventType::RetargetEventTypeGenerated);
  DCHECK(parent) << "RetargetForEvents shouldn't return a "
                    "null pointer when |parent| is not null.";
  parent_manager->FireGeneratedEvent(
      ui::AXEventGenerator::Event::CHILDREN_CHANGED, parent);
}

void AXTreeManager::EnsureParentConnectionIfNotRootManager() {
  AXNode* parent = GetParentNodeFromParentTree();
  if (parent) {
    if (!connected_to_parent_tree_node_)
      ParentConnectionChanged(parent);
    SANITIZER_CHECK(!IsRoot());
    return;
  }

  if (connected_to_parent_tree_node_) {
    connected_to_parent_tree_node_ = false;
    // Two possible cases:
    // 1. This manager was previously connected to a parent manager but now
    // became the new root manager.
    // 2. The parent host node for this child tree was removed. Because the
    // connection with the root has been severed, it will no longer be possible
    // to fire events, as this AXTreeManager is no longer tied to
    // an existing UI element. Due to race conditions, in some cases, `this` is
    // destroyed first, and this condition is not reached; while in other cases
    // the parent node is destroyed first (this case).
    DCHECK(IsRoot() || !CanFireEvents());
  }
}

}  // namespace ui
