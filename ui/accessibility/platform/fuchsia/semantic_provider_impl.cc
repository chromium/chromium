// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/fuchsia/semantic_provider_impl.h"

#include <fidl/fuchsia.ui.gfx/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/sys/cpp/component_context.h>

#include "base/check.h"
#include "base/check_op.h"
#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "ui/gfx/geometry/transform.h"

namespace ui {
namespace {

using fuchsia_accessibility_semantics::Node;

constexpr size_t kMaxOperationsPerBatch = 16;

SemanticTreeEventHandler::SemanticTreeEventHandler(
    base::OnceCallback<void(fidl::UnbindInfo)> on_fidl_error_callback)
    : on_fidl_error_callback_(std::move(on_fidl_error_callback)) {}
SemanticTreeEventHandler::~SemanticTreeEventHandler() = default;

void SemanticTreeEventHandler::on_fidl_error(fidl::UnbindInfo error) {
  std::move(on_fidl_error_callback_).Run(error);
}

}  // namespace

AXFuchsiaSemanticProviderImpl::Batch::Batch(Type type) : type_(type) {}
AXFuchsiaSemanticProviderImpl::Batch::Batch(Batch&& other) = default;
AXFuchsiaSemanticProviderImpl::Batch::~Batch() = default;

bool AXFuchsiaSemanticProviderImpl::Batch::IsFull() const {
  return (
      (type_ == Type::kUpdate && updates_.size() >= kMaxOperationsPerBatch) ||
      (type_ == Type::kDelete &&
       delete_node_ids_.size() >= kMaxOperationsPerBatch));
}

void AXFuchsiaSemanticProviderImpl::Batch::Append(
    fuchsia_accessibility_semantics::Node node) {
  DCHECK_EQ(type_, Type::kUpdate);
  DCHECK(!IsFull());
  updates_.push_back(std::move(node));
}

void AXFuchsiaSemanticProviderImpl::Batch::AppendDeletion(
    uint32_t delete_node_id) {
  DCHECK_EQ(type_, Type::kDelete);
  DCHECK(!IsFull());
  delete_node_ids_.push_back(delete_node_id);
}

void AXFuchsiaSemanticProviderImpl::Batch::Apply(
    fidl::Client<fuchsia_accessibility_semantics::SemanticTree>*
        semantic_tree) {
  if (type_ == Type::kUpdate && !updates_.empty()) {
    auto result = (*semantic_tree)->UpdateSemanticNodes(std::move(updates_));
    LOG_IF(ERROR, result.is_error())
        << base::FidlMethodResultErrorMessage(result, "UpdateSemanticNodes");
  } else if (type_ == Type::kDelete && !delete_node_ids_.empty()) {
    auto result =
        (*semantic_tree)->DeleteSemanticNodes(std::move(delete_node_ids_));
    LOG_IF(ERROR, result.is_error())
        << base::FidlMethodResultErrorMessage(result, "DeleteSemanticNodes");
  }
}

AXFuchsiaSemanticProviderImpl::NodeInfo ::NodeInfo() = default;
AXFuchsiaSemanticProviderImpl::NodeInfo ::~NodeInfo() = default;

AXFuchsiaSemanticProviderImpl::Delegate::Delegate() = default;
AXFuchsiaSemanticProviderImpl::Delegate::~Delegate() = default;

AXFuchsiaSemanticProviderImpl::AXFuchsiaSemanticProviderImpl(
    fuchsia_ui_views::ViewRef view_ref,
    Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);

  auto semantics_manager_client_end = base::fuchsia_component::Connect<
      fuchsia_accessibility_semantics::SemanticsManager>();
  // TODO(crbug.com/40263576): Create a path for gracefully failing to connect
  // to SemanticsManager instead of CHECKing.
  CHECK(semantics_manager_client_end.is_ok())
      << base::FidlConnectionErrorMessage(semantics_manager_client_end);
  fidl::Client semantics_manager(
      std::move(semantics_manager_client_end.value()),
      async_get_default_dispatcher());

  auto semantic_listener_endpoints = fidl::CreateEndpoints<
      fuchsia_accessibility_semantics::SemanticListener>();
  ZX_CHECK(semantic_listener_endpoints.is_ok(),
           semantic_listener_endpoints.status_value());
  semantic_listener_binding_.emplace(
      async_get_default_dispatcher(),
      std::move(semantic_listener_endpoints->server), this,
      base::FidlBindingClosureWarningLogger(
          "fuchsia.accessibility.semantics.SemanticListener"));

  semantic_tree_event_handler_.emplace(base::BindOnce(
      [](AXFuchsiaSemanticProviderImpl* semantic_provider,
         fidl::UnbindInfo info) {
        ZX_LOG(ERROR, info.status()) << "SemanticListener disconnected";
        semantic_provider->delegate_->OnSemanticsManagerConnectionClosed(
            info.status());
        semantic_provider->semantic_updates_enabled_ = false;
      },
      this));

  auto semantic_tree_endpoints =
      fidl::CreateEndpoints<fuchsia_accessibility_semantics::SemanticTree>();
  ZX_CHECK(semantic_tree_endpoints.is_ok(),
           semantic_tree_endpoints.status_value());
  semantic_tree_.Bind(std::move(semantic_tree_endpoints->client),
                      async_get_default_dispatcher(),
                      &semantic_tree_event_handler_.value());

  auto result = semantics_manager->RegisterViewForSemantics({{
      .view_ref = std::move(view_ref),
      .listener = std::move(semantic_listener_endpoints->client),
      .semantic_tree_request = std::move(semantic_tree_endpoints->server),
  }});
  if (result.is_error()) {
    ZX_LOG(ERROR, result.error_value().status())
        << "Error calling RegisterViewForSemantics()";
  }
}

AXFuchsiaSemanticProviderImpl::~AXFuchsiaSemanticProviderImpl() = default;

bool AXFuchsiaSemanticProviderImpl::Update(
    fuchsia_accessibility_semantics::Node node) {
  if (!semantic_updates_enabled())
    return false;

  DCHECK(node.node_id().has_value());

  // If the updated node is the root, we need to account for the pixel scale in
  // its transform.
  //
  // Otherwise, we need to update our connectivity book-keeping.
  if (node.node_id() == kFuchsiaRootNodeId) {
    gfx::Transform transform;
    transform.PostScale(1 / pixel_scale_, 1 / pixel_scale_);

    // Convert to fuchsia's transform type.
    std::array<float, 16> mat = {};
    transform.GetColMajorF(mat.data());
    fuchsia_ui_gfx::Mat4 mat4{std::move(mat)};
    // The root node will never have an offset container, so its transform will
    // always be the identity matrix. Thus, we can safely overwrite it here.
    node.node_to_container_transform(std::move(mat4));
  } else {
    auto found_not_reachable = not_reachable_.find(node.node_id().value());
    const bool is_not_reachable = found_not_reachable != not_reachable_.end();
    const std::optional<uint32_t> parent_node_id =
        GetParentForNode(node.node_id().value());
    if (is_not_reachable && parent_node_id) {
      // Connection parent -> |node| exists now.
      not_reachable_.erase(found_not_reachable);
      nodes_[node.node_id().value()].parents.insert(*parent_node_id);
    } else if (!parent_node_id) {
      // No node or multiple nodes points to this one, so it is not reachable.
      if (!is_not_reachable)
        not_reachable_[node.node_id().value()] = {};
    }
  }

  // If the node is not present in the map, the list of children will be empty
  // so this is a no-op in the call below.
  std::vector<uint32_t>& children = nodes_[node.node_id().value()].children;

  // Before updating the node, update the list of children to be not reachable,
  // in case the new list of children change.
  MarkChildrenAsNotReachable(children, node.node_id().value());
  children = node.child_ids().value_or(std::vector<uint32_t>());
  MarkChildrenAsReachable(children, node.node_id().value());

  Batch& batch = GetCurrentUnfilledBatch(Batch::Type::kUpdate);
  batch.Append(std::move(node));
  TryToCommit();
  return true;
}

void AXFuchsiaSemanticProviderImpl::TryToCommit() {
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
  semantic_tree_->CommitUpdates().Then(
      fit::bind_member(this, &AXFuchsiaSemanticProviderImpl::OnCommitComplete));
  commit_inflight_ = true;
}

bool AXFuchsiaSemanticProviderImpl::Delete(uint32_t node_id) {
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

void AXFuchsiaSemanticProviderImpl::SendEvent(
    fuchsia_accessibility_semantics::SemanticEvent event) {
  semantic_tree_->SendSemanticEvent({{.semantic_event = std::move(event)}})
      .Then(
          [](fidl::Result<
              fuchsia_accessibility_semantics::SemanticTree::SendSemanticEvent>&
                 result) {
            ZX_LOG_IF(ERROR, result.is_error(), result.error_value().status());
          });
}

bool AXFuchsiaSemanticProviderImpl::HasPendingUpdates() const {
  return commit_inflight_ || !batches_.empty();
}

bool AXFuchsiaSemanticProviderImpl::Clear() {
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

void AXFuchsiaSemanticProviderImpl::OnAccessibilityActionRequested(
    AXFuchsiaSemanticProviderImpl::OnAccessibilityActionRequestedRequest&
        request,
    AXFuchsiaSemanticProviderImpl::OnAccessibilityActionRequestedCompleter::
        Sync& completer) {
  if (delegate_->OnAccessibilityAction(request.node_id(), request.action())) {
    completer.Reply(true);
    return;
  }

  // The action was not handled.
  completer.Reply(false);
}

void AXFuchsiaSemanticProviderImpl::HitTest(
    AXFuchsiaSemanticProviderImpl::HitTestRequest& request,
    AXFuchsiaSemanticProviderImpl::HitTestCompleter::Sync& completer) {
  delegate_->OnHitTest(
      {{
          .x = request.local_point().x() * pixel_scale_,
          .y = request.local_point().y() * pixel_scale_,
      }},
      base::BindOnce(
          [](HitTestCompleter::Async async_completer,
             const fidl::Response<
                 fuchsia_accessibility_semantics::SemanticListener::HitTest>&
                 result) { async_completer.Reply(result); },
          completer.ToAsync()));
  return;
}

void AXFuchsiaSemanticProviderImpl::OnSemanticsModeChanged(
    AXFuchsiaSemanticProviderImpl::OnSemanticsModeChangedRequest& request,
    AXFuchsiaSemanticProviderImpl::OnSemanticsModeChangedCompleter::Sync&
        completer) {
  if (semantic_updates_enabled_ != request.updates_enabled()) {
    delegate_->OnSemanticsEnabled(request.updates_enabled());
  }

  semantic_updates_enabled_ = request.updates_enabled();
  completer.Reply();
}

void AXFuchsiaSemanticProviderImpl::MarkChildrenAsNotReachable(
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

void AXFuchsiaSemanticProviderImpl::MarkChildrenAsReachable(
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

std::optional<uint32_t> AXFuchsiaSemanticProviderImpl::GetParentForNode(
    const uint32_t node_id) {
  const auto it = nodes_.find(node_id);
  if (it != nodes_.end()) {
    if (it->second.parents.size() == 1)
      return *it->second.parents.begin();
    else
      return std::nullopt;
  }

  const auto not_reachable_it = not_reachable_.find(node_id);
  if (not_reachable_it != not_reachable_.end()) {
    if (not_reachable_it->second.size() == 1)
      return *not_reachable_it->second.begin();
    else
      return std::nullopt;
  }

  return std::nullopt;
}

AXFuchsiaSemanticProviderImpl::Batch&
AXFuchsiaSemanticProviderImpl::GetCurrentUnfilledBatch(Batch::Type type) {
  if (batches_.empty() || batches_.back().type() != type ||
      batches_.back().IsFull())
    batches_.emplace_back(type);

  return batches_.back();
}

void AXFuchsiaSemanticProviderImpl::OnCommitComplete(
    fidl::Result<fuchsia_accessibility_semantics::SemanticTree::CommitUpdates>&
        result) {
  ZX_LOG_IF(ERROR, result.is_error(), result.error_value().status());
  commit_inflight_ = false;
  TryToCommit();
}

float AXFuchsiaSemanticProviderImpl::GetPixelScale() const {
  return pixel_scale_;
}

void AXFuchsiaSemanticProviderImpl::SetPixelScale(float pixel_scale) {
  pixel_scale_ = pixel_scale;

  // If the root node exists, then we need to update its transform to reflect
  // the new pixel scale.
  if (nodes_.find(kFuchsiaRootNodeId) == nodes_.end())
    return;

  // We need to fill the `child_ids` field to prevent Update() from trampling
  // our connectivity bookkeeping. Update() will handle setting the
  // `node_to_container_transform` field.
  Update({{
      .node_id = kFuchsiaRootNodeId,
      .child_ids = nodes_[kFuchsiaRootNodeId].children,
  }});
}

}  // namespace ui
