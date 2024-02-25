// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_FUCHSIA_SEMANTIC_PROVIDER_IMPL_H_
#define UI_ACCESSIBILITY_PLATFORM_FUCHSIA_SEMANTIC_PROVIDER_IMPL_H_

#include <fidl/fuchsia.accessibility.semantics/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include <map>
#include <optional>
#include <set>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "ui/accessibility/platform/fuchsia/semantic_provider.h"

namespace ui {

namespace {

class SemanticTreeEventHandler
    : public fidl::AsyncEventHandler<
          fuchsia_accessibility_semantics::SemanticTree> {
 public:
  explicit SemanticTreeEventHandler(
      base::OnceCallback<void(fidl::UnbindInfo)> on_fidl_error_callback);
  ~SemanticTreeEventHandler() override;

  void on_fidl_error(fidl::UnbindInfo error) override;

 private:
  base::OnceCallback<void(fidl::UnbindInfo)> on_fidl_error_callback_;
};

}  // namespace

// Clients instantiate this class, which connects to the Fuchsia semantics API.
// This object must remain alive across the entire lifespan of the corresponding
// fuchsia view.
class COMPONENT_EXPORT(AX_PLATFORM) AXFuchsiaSemanticProviderImpl
    : public AXFuchsiaSemanticProvider,
      public fidl::Server<fuchsia_accessibility_semantics::SemanticListener> {
 public:
  // Arguments:
  // |view_ref|: identifies the view providing semantics. Please consult
  // |fuchsia.accessibility.semantics| API documentation.
  // |delegate|: Handles semantic requests, please see Delegate class for more
  // documentation. Caller is responsible for ensuring that |delegate| outlives
  // |this|.
  // During construction, this class connects to
  // |fuchsia.accessibility.semantics.SemanticsManager| to register itself as a
  // semantic provider.
  AXFuchsiaSemanticProviderImpl(fuchsia_ui_views::ViewRef view_ref,
                                Delegate* delegate);
  ~AXFuchsiaSemanticProviderImpl() override;

  // Returns true if Fuchsia has enabled semantics.
  bool semantic_updates_enabled() const { return semantic_updates_enabled_; }

  // AXFuchsiaSemanticProvider overrides.
  bool Update(fuchsia_accessibility_semantics::Node node) override;
  bool Delete(uint32_t node_id) override;
  bool Clear() override;
  void SendEvent(fuchsia_accessibility_semantics::SemanticEvent event) override;
  bool HasPendingUpdates() const override;
  float GetPixelScale() const override;
  void SetPixelScale(float pixel_scale) override;

 private:
  // Holds information about a Fuchsia Semantic Node. It contains only the
  // fields needed to check that the resulting tree would be valid.
  struct NodeInfo {
    NodeInfo();
    ~NodeInfo();

    // During a tree update a node may have multiple parents pointing to it,
    // although after all updates are processed only one should be present.
    std::set<uint32_t> parents;
    std::vector<uint32_t> children;
  };

  // Represents a batch of nodes to be sent to Fuchsia.
  // Batches can hold exactly one type: a series of updates or a series of
  // deletions.
  class Batch {
   public:
    enum class Type { kUpdate, kDelete };

    Batch(Type type);
    Batch(Batch&& other);
    ~Batch();
    Batch(const Batch& other) = delete;

    Type type() const { return type_; }

    // Returns true if the batch has reached its size limit.
    bool IsFull() const;

    // Adds an update or deletion to the batch. This fails if the batch is full
    // or if the new item is not the same type of the batch.
    void Append(fuchsia_accessibility_semantics::Node node);
    void AppendDeletion(uint32_t delete_node_id);

    // Sends enqueued operations to SemanticsManager.
    void Apply(fidl::Client<fuchsia_accessibility_semantics::SemanticTree>*
                   semantic_tree);

   private:
    Type type_;
    std::vector<fuchsia_accessibility_semantics::Node> updates_;
    std::vector<uint32_t> delete_node_ids_;
  };

  // Attempts to commit the pending updates to Fuchsia if the resulting updates
  // would leave the final tree in a valid state.
  void TryToCommit();

  // Returns a batch that can receive an update or deletion depending on |type|.
  Batch& GetCurrentUnfilledBatch(Batch::Type type);

  // Invoked whenever Fuchsia responds that a commit was received. This tries to
  // commit again if there are pending upedates or deletions.
  void OnCommitComplete(
      fidl::Result<
          fuchsia_accessibility_semantics::SemanticTree::CommitUpdates>&
          result);

  // Mark all |child_ids| not reachable from |parent_id|, meaning:
  // - If |parent_id| was the only parent, the children are now disconnected
  // from the tree.
  // - If |parent_id| was an additional parent, now the children are connected
  // to a single parent in the tree.
  // - If the children do not exist, remove them from the list of nodes waiting
  // to be updated.
  void MarkChildrenAsNotReachable(const std::vector<uint32_t>& child_ids,
                                  uint32_t parent_id);

  // Mark all |child_ids| reachable from |parent_id|, meaning:
  // - If |parent_id| is the only parent, the children are now connected to the
  // tree and are all reachable.
  // - If |parent_id| is an additional parent, now the children are not
  // connected to the tree as multiple parents point to them.
  // - If the children do not exist, the parent waits for the nodes to be
  // created.
  void MarkChildrenAsReachable(const std::vector<uint32_t>& child_ids,
                               uint32_t parent_id);

  // Returns the ID of the parent of this node if it has one. If it does not
  // have a parent or it has multiple parents, returns std::nullopt.
  std::optional<uint32_t> GetParentForNode(const uint32_t node_id);

  // fuchsia_accessibility_semantics::SemanticListener:
  void OnAccessibilityActionRequested(
      OnAccessibilityActionRequestedRequest& request,
      OnAccessibilityActionRequestedCompleter::Sync& completer) override;

  // fuchsia_accessibility_semantics::SemanticListener:
  void HitTest(HitTestRequest& request,
               HitTestCompleter::Sync& completer) override;

  // fuchsia_accessibility_semantics::SemanticListener:
  void OnSemanticsModeChanged(
      OnSemanticsModeChangedRequest& request,
      OnSemanticsModeChangedCompleter::Sync& completer) override;

  Delegate* const delegate_;

  std::optional<
      fidl::ServerBinding<fuchsia_accessibility_semantics::SemanticListener>>
      semantic_listener_binding_;

  fidl::Client<fuchsia_accessibility_semantics::SemanticTree> semantic_tree_;

  std::optional<SemanticTreeEventHandler> semantic_tree_event_handler_;

  bool semantic_updates_enabled_ = false;

  // Nodes from this tree. If not empty, to be considered a valid tree, there
  // must be:
  // - A node which node id is equal to kFuchsiaRootNodeId;
  // - Each node except the root has only one parent;
  // - All children pointed by a parent exist in the tree.
  // Only the node ID and the child IDs of the node are stored here because at
  // this point we only check to see if the tree is valid.
  std::map<uint32_t /*node_id*/, NodeInfo> nodes_;

  // Key == the node ID that is not reachable from the root of the tree, value
  // == 0 or more parents that point to this node. Note that nodes can be listed
  // here but still be present in |nodes_|. This may happen, for example, if the
  // parent of the node was deleted and there is no path from the root to it, so
  // the node waits for a parent to connect to it.
  std::map<uint32_t, std::set<uint32_t>> not_reachable_;

  // Stores batches of node updates or deletions to be sent to Fuchsia. Note
  // that a batch contains only updates or deletions, because they are pushed to
  // Fuchsia differently.
  std::vector<Batch> batches_;

  bool commit_inflight_ = false;

  // The scale factor used to convert between the coordinate space chrome
  // allocates for the view and the view's logical size reported by scenic.
  float pixel_scale_ = 1.f;
};

}  // namespace ui
#endif  // UI_ACCESSIBILITY_PLATFORM_FUCHSIA_SEMANTIC_PROVIDER_IMPL_H_
