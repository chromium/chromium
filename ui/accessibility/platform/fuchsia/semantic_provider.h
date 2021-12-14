// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_FUCHSIA_SEMANTIC_PROVIDER_H_
#define UI_ACCESSIBILITY_PLATFORM_FUCHSIA_SEMANTIC_PROVIDER_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include <map>
#include <set>
#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

// Manages the connection with the Fuchsia Semantics API.
//
// Clients instantiate this class, which connects to the Fuchsia semantics API.
// Semantic nodes can be added or deleted. When a batch of nodes would leave the
// Fuchsia semantic tree in a valid state, they are committed. Please see
// |fuchsia.accessibility.semantics| API for more documentation on valid
// semantic trees.
class AX_EXPORT AXFuchsiaSemanticProvider
    : public fuchsia::accessibility::semantics::SemanticListener {
 public:
  // A delegate that can be registered by clients of this library to be notified
  // about Semantic changes.
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    // Called when the FIDL channel to the Semantics Manager is closed. If this
    // callback returns true, an attempt to reconnect will be made.
    virtual bool OnSemanticsManagerConnectionClosed() = 0;

    // Processes an incoming accessibility action from Fuchsia. It
    // receives the Fuchsia node ID and the action requested. If this
    // method returns true, this means that the action will be handled.
    virtual bool OnAccessibilityAction(
        uint32_t node_id,
        fuchsia::accessibility::semantics::Action action) = 0;

    // Processes an incoming hit test request from Fuchsia. It
    // receives a point in Scenic View pixel coordinates and a callback to
    // return the result when the hit test is done. Please see
    // |fuchsia.accessibility.semantics.SemanticListener| for documentation on
    // hit tests.
    virtual void OnHitTest(fuchsia::math::PointF point,
                           HitTestCallback callback) = 0;

    // Called whenever Fuchsia enables / disables semantic updates.
    virtual void OnSemanticsEnabled(bool enabled) = 0;
  };

  // Arguments:
  // |view_ref|: identifies the view providing semantics. Please consult
  // |fuchsia.accessibility.semantics| API documentation.
  // |pixel_scale|: Scales Scenic view coordinates to pixel coordinates.
  // |delegate|: Handles semantic requests, please see Delegate class for more
  // documentation. Caller is responsible for ensuring that |delegate| outlives
  // |this|.
  // During construction, this class connects to
  // |fuchsia.accessibility.semantics.SemanticsManager| to register itself as a
  // semantic provider.
  AXFuchsiaSemanticProvider(fuchsia::ui::views::ViewRef view_ref,
                            float pixel_scale,
                            Delegate* delegate);
  ~AXFuchsiaSemanticProvider() override;

  // Returns true if Fuchsia has enabled semantics.
  bool semantic_updates_enabled() const { return semantic_updates_enabled_; }

  // Adds a semantic node to be updated. It is mandatory that the node has at
  // least an unique ID.
  bool Update(fuchsia::accessibility::semantics::Node node);

  // Marks a semantic node to be deleted. Returns false if the node is not
  // present in the list of semantic nodes known by this provider.
  bool Delete(uint32_t node_id);

  // Clears the semantic tree.
  bool Clear();

  // Sends an accessibility event to Fuchsia. Please consult
  // https://cs.opensource.google/fuchsia/fuchsia/+/master:sdk/fidl/fuchsia.accessibility.semantics/semantics_manager.fidl
  // for documentation on events.
  void SendEvent(fuchsia::accessibility::semantics::SemanticEvent event);

  // Returns true if there are pending updates or deletions to be made.
  bool HasPendingUpdates() const;

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
    void Append(fuchsia::accessibility::semantics::Node node);
    void AppendDeletion(uint32_t delete_node_id);

    // Sends enqueued operations to SemanticsManager.
    void Apply(
        fuchsia::accessibility::semantics::SemanticTreePtr* semantic_tree);

   private:
    Type type_;
    std::vector<fuchsia::accessibility::semantics::Node> updates_;
    std::vector<uint32_t> delete_node_ids_;
  };

  // Attempts to commit the pending updates to Fuchsia if the resulting updates
  // would leave the final tree in a valid state.
  void TryToCommit();

  // Returns a batch that can receive an update or deletion depending on |type|.
  Batch& GetCurrentUnfilledBatch(Batch::Type type);

  // Invoked whenever Fuchsia responds that a commit was received. This tries to
  // commit again if there are pending upedates or deletions.
  void OnCommitComplete();

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
  // have a parent or it has multiple parents, returns absl::nullopt.
  absl::optional<uint32_t> GetParentForNode(const uint32_t node_id);

  // fuchsia::accessibility::semantics::SemanticListener:
  void OnAccessibilityActionRequested(
      uint32_t node_id,
      fuchsia::accessibility::semantics::Action action,
      fuchsia::accessibility::semantics::SemanticListener::
          OnAccessibilityActionRequestedCallback callback) override;

  // fuchsia::accessibility::semantics::SemanticListener:
  void HitTest(::fuchsia::math::PointF local_point,
               HitTestCallback callback) override;

  // fuchsia::accessibility::semantics::SemanticListener:
  void OnSemanticsModeChanged(bool update_enabled,
                              OnSemanticsModeChangedCallback callback) override;

  fuchsia::ui::views::ViewRef view_ref_;
  float pixel_scale_;
  Delegate* const delegate_;

  fidl::Binding<fuchsia::accessibility::semantics::SemanticListener>
      semantic_listener_binding_;

  fuchsia::accessibility::semantics::SemanticTreePtr semantic_tree_;

  bool semantic_updates_enabled_ = true;

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

  bool commit_inflight_;
};

}  // namespace ui
#endif  // UI_ACCESSIBILITY_PLATFORM_FUCHSIA_SEMANTIC_PROVIDER_H_
