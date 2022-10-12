// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_manager.h"

#include "base/lazy_instance.h"
#include "base/no_destructor.h"
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
         !child_tree_manager->GetParentNodeFromParentTreeAsAXNode() ||
         child_tree_manager->GetParentNodeFromParentTreeAsAXNode()->id() ==
             parent_node.id());
  return child_tree_manager;
}

// static
void AXTreeManager::SetFocusChangeCallbackForTesting(
    base::RepeatingClosure callback) {
  g_focus_change_callback_for_testing.Get() = std::move(callback);
}

AXTreeManager::AXTreeManager()
    : ax_tree_id_(AXTreeIDUnknown()),
      ax_tree_(nullptr),
      event_generator_(ax_tree()) {}

AXTreeManager::AXTreeManager(std::unique_ptr<AXTree> tree)
    : ax_tree_id_(tree ? tree->data().tree_id : AXTreeIDUnknown()),
      ax_tree_(std::move(tree)),
      event_generator_(ax_tree()) {
  GetMap().AddTreeManager(ax_tree_id_, this);
  if (ax_tree())
    tree_observation_.Observe(ax_tree());
}

AXTreeManager::AXTreeManager(const AXTreeID& tree_id,
                             std::unique_ptr<AXTree> tree)
    : ax_tree_id_(tree_id),
      ax_tree_(std::move(tree)),
      event_generator_(ax_tree()) {
  GetMap().AddTreeManager(ax_tree_id_, this);
  if (ax_tree())
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
  // Stop observing so we don't get a callback for every node being deleted.
  event_generator_.ReleaseTree();
  if (ax_tree_)
    GetMap().RemoveTreeManager(ax_tree_id_);

  if (last_focused_node_tree_id_ && ax_tree_id_ == *last_focused_node_tree_id_)
    SetLastFocusedNode(nullptr);
}

void AXTreeManager::OnTreeDataChanged(AXTree* tree,
                                      const AXTreeData& old_data,
                                      const AXTreeData& new_data) {
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

void AXTreeManager::RemoveFromMap() {
  GetMap().RemoveTreeManager(ax_tree_id_);
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

}  // namespace ui
