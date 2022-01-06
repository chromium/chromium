// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/fuchsia/semantic_provider.h"

#include <lib/sys/cpp/component_context.h>

#include "base/check.h"
#include "base/check_op.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"

namespace ui {
namespace {

using fuchsia::accessibility::semantics::Node;

constexpr uint32_t kFuchsiaRootNodeId = 0;
constexpr size_t kMaxOperationsPerBatch = 16;

}  // namespace

AXFuchsiaSemanticProvider::Batch::Batch(Type type) : type_(type) {}
AXFuchsiaSemanticProvider::Batch::Batch(Batch&& other) = default;
AXFuchsiaSemanticProvider::Batch::~Batch() = default;

bool AXFuchsiaSemanticProvider::Batch::IsFull() const {
  return (
      (type_ == Type::kUpdate && updates_.size() >= kMaxOperationsPerBatch) ||
      (type_ == Type::kDelete &&
       delete_node_ids_.size() >= kMaxOperationsPerBatch));
}

void AXFuchsiaSemanticProvider::Batch::Append(
    fuchsia::accessibility::semantics::Node node) {
  DCHECK_EQ(type_, Type::kUpdate);
  DCHECK(!IsFull());
  updates_.push_back(std::move(node));
}

void AXFuchsiaSemanticProvider::Batch::AppendDeletion(uint32_t delete_node_id) {
  DCHECK_EQ(type_, Type::kDelete);
  DCHECK(!IsFull());
  delete_node_ids_.push_back(delete_node_id);
}

void AXFuchsiaSemanticProvider::Batch::Apply(
    fuchsia::accessibility::semantics::SemanticTreePtr* semantic_tree) {
  if (type_ == Type::kUpdate && !updates_.empty())
    (*semantic_tree)->UpdateSemanticNodes(std::move(updates_));
  else if (type_ == Type::kDelete && !delete_node_ids_.empty())
    (*semantic_tree)->DeleteSemanticNodes(std::move(delete_node_ids_));
}

AXFuchsiaSemanticProvider::NodeInfo ::NodeInfo() = default;
AXFuchsiaSemanticProvider::NodeInfo ::~NodeInfo() = default;

AXFuchsiaSemanticProvider::Delegate::Delegate() = default;
AXFuchsiaSemanticProvider::Delegate::~Delegate() = default;

AXFuchsiaSemanticProvider::AXFuchsiaSemanticProvider(
    fuchsia::ui::views::ViewRef view_ref,
    float pixel_scale,
    Delegate* delegate)
    : view_ref_(std::move(view_ref)),
      pixel_scale_(pixel_scale),
      delegate_(delegate),
      semantic_listener_binding_(this) {
  sys::ComponentContext* component_context = base::ComponentContextForProcess();
  DCHECK(component_context);
  DCHECK(delegate_);

  component_context->svc()
      ->Connect<fuchsia::accessibility::semantics::SemanticsManager>()
      ->RegisterViewForSemantics(std::move(view_ref_),
                                 semantic_listener_binding_.NewBinding(),
                                 semantic_tree_.NewRequest());
  semantic_tree_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "SemanticTree disconnected";
    delegate_->OnSemanticsManagerConnectionClosed();
    semantic_updates_enabled_ = false;
  });
}

AXFuchsiaSemanticProvider::~AXFuchsiaSemanticProvider() = default;

bool AXFuchsiaSemanticProvider::Update(
    fuchsia::accessibility::semantics::Node node) {
  if (!semantic_updates_enabled())
    return false;

  DCHECK(node.has_node_id());

  if (node.node_id() != kFuchsiaRootNodeId) {
    auto found_not_reachable = not_reachable_.find(node.node_id());
    const bool is_not_reachable = found_not_reachable != not_reachable_.end();
    const absl::optional<uint32_t> parent_node_id =
        GetParentForNode(node.node_id());
    if (is_not_reachable && parent_node_id) {
      // Connection parent -> |node| exists now.
      not_reachable_.erase(found_not_reachable);
      nodes_[node.node_id()].parents.insert(*parent_node_id);
    } else if (!parent_node_id) {
      // No node or multiple nodes points to this one, so it is not reachable.
      if (!is_not_reachable)
        not_reachable_[node.node_id()] = {};
    }
  }

  // If the node is not present in the map, the list of children will be empty
  // so this is a no-op in the call below.
  std::vector<uint32_t>& children = nodes_[node.node_id()].children;

  // Before updating the node, update the list of children to be not reachable,
  // in case the new list of children change.
  MarkChildrenAsNotReachable(children, node.node_id());
  children = node.has_child_ids() ? node.child_ids() : std::vector<uint32_t>();
  MarkChildrenAsReachable(children, node.node_id());

  Batch& batch = GetCurrentUnfilledBatch(Batch::Type::kUpdate);
  batch.Append(std::move(node));
  TryToCommit();
  return true;
}

void AXFuchsiaSemanticProvider::TryToCommit() {
  // Don't send out updates while the tree is mid-mutation.
  if (commit_inflight_ || batches_.empty())
    return;

  // If a tree has nodes but no root, wait until the root is present or all
  // nodes are deleted.
  if (!nodes_.empty() && nodes_.find(kFuchsiaRootNodeId) == nodes_.end())
    return;

  if (!not_reachable_.empty())
    return;

  for (auto& batch : batches_) {
    batch.Apply(&semantic_tree_);
  }

  batches_.clear();
  semantic_tree_->CommitUpdates(
      fit::bind_member(this, &AXFuchsiaSemanticProvider::OnCommitComplete));
  commit_inflight_ = true;
}

bool AXFuchsiaSemanticProvider::Delete(uint32_t node_id) {
  if (!semantic_updates_enabled())
    return false;

  auto it = nodes_.find(node_id);
  if (it == nodes_.end())
    return false;

  if (it->second.parents.empty()) {
    // No node points to this one, so it is safe to remove it from the tree.
    not_reachable_.erase(node_id);
  } else {
    not_reachable_[node_id] =
        it->second
            .parents;  // Zero or more parents can be pointing to this node.
  }
  MarkChildrenAsNotReachable(it->second.children, node_id);

  nodes_.erase(it);

  Batch& batch = GetCurrentUnfilledBatch(Batch::Type::kDelete);
  batch.AppendDeletion(node_id);
  TryToCommit();
  return true;
}

void AXFuchsiaSemanticProvider::SendEvent(
    fuchsia::accessibility::semantics::SemanticEvent event) {
  semantic_tree_->SendSemanticEvent(std::move(event), [](auto...) {});
}

bool AXFuchsiaSemanticProvider::HasPendingUpdates() const {
  return commit_inflight_ || !batches_.empty();
}

bool AXFuchsiaSemanticProvider::Clear() {
  if (!semantic_updates_enabled())
    return false;

  batches_.clear();
  not_reachable_.clear();
  nodes_.clear();
  Batch& batch = GetCurrentUnfilledBatch(Batch::Type::kDelete);
  batch.AppendDeletion(kFuchsiaRootNodeId);
  TryToCommit();
  return true;
}

void AXFuchsiaSemanticProvider::OnAccessibilityActionRequested(
    uint32_t node_id,
    fuchsia::accessibility::semantics::Action action,
    fuchsia::accessibility::semantics::SemanticListener::
        OnAccessibilityActionRequestedCallback callback) {
  if (delegate_->OnAccessibilityAction(node_id, action)) {
    callback(true);
    return;
  }

  // The action was not handled.
  callback(false);
}

void AXFuchsiaSemanticProvider::HitTest(fuchsia::math::PointF local_point,
                                        HitTestCallback callback) {
  fuchsia::math::PointF point;
  point.x = local_point.x * pixel_scale_;
  point.y = local_point.y * pixel_scale_;

  delegate_->OnHitTest(point, std::move(callback));
  return;
}

void AXFuchsiaSemanticProvider::OnSemanticsModeChanged(
    bool update_enabled,
    OnSemanticsModeChangedCallback callback) {
  if (semantic_updates_enabled_ != update_enabled)
    delegate_->OnSemanticsEnabled(update_enabled);

  semantic_updates_enabled_ = update_enabled;
  callback();
}

void AXFuchsiaSemanticProvider::MarkChildrenAsNotReachable(
    const std::vector<uint32_t>& child_ids,
    uint32_t parent_id) {
  for (const uint32_t child_id : child_ids) {
    const auto it = nodes_.find(child_id);
    if (it != nodes_.end()) {
      it->second.parents.erase(parent_id);
      if (it->second.parents.empty())
        not_reachable_[child_id] = {};
      else
        not_reachable_.erase(child_id);
    } else {
      auto not_reachable_it = not_reachable_.find(child_id);
      // Child id is no longer in the regular map, deletes it also from
      // not_reachable_ if no parent points to it anymore.
      if (not_reachable_it != not_reachable_.end()) {
        not_reachable_it->second.erase(parent_id);
        if (not_reachable_it->second.empty())
          not_reachable_.erase(not_reachable_it);
      }
    }
  }
}

void AXFuchsiaSemanticProvider::MarkChildrenAsReachable(
    const std::vector<uint32_t>& child_ids,
    uint32_t parent_id) {
  for (const uint32_t child_id : child_ids) {
    auto it = nodes_.find(child_id);
    if (it == nodes_.end())
      not_reachable_[child_id].insert(parent_id);
    else {
      it->second.parents.insert(parent_id);
      if (it->second.parents.size() == 1)
        not_reachable_.erase(child_id);
      else
        not_reachable_[child_id].insert(parent_id);
    }
  }
}

absl::optional<uint32_t> AXFuchsiaSemanticProvider::GetParentForNode(
    const uint32_t node_id) {
  const auto it = nodes_.find(node_id);
  if (it != nodes_.end()) {
    if (it->second.parents.size() == 1)
      return *it->second.parents.begin();
    else
      return absl::nullopt;
  }

  const auto not_reachable_it = not_reachable_.find(node_id);
  if (not_reachable_it != not_reachable_.end()) {
    if (not_reachable_it->second.size() == 1)
      return *not_reachable_it->second.begin();
    else
      return absl::nullopt;
  }

  return absl::nullopt;
}

AXFuchsiaSemanticProvider::Batch&
AXFuchsiaSemanticProvider::GetCurrentUnfilledBatch(Batch::Type type) {
  if (batches_.empty() || batches_.back().type() != type ||
      batches_.back().IsFull())
    batches_.push_back(Batch(type));

  return batches_.back();
}

void AXFuchsiaSemanticProvider::OnCommitComplete() {
  commit_inflight_ = false;
  TryToCommit();
}

}  // namespace ui
