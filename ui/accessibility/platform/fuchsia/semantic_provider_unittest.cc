// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "semantic_provider_impl.h"

#include <fidl/fuchsia.accessibility.semantics/cpp/fidl.h>
#include <fidl/fuchsia.ui.views/cpp/hlcpp_conversion.h>
#include <lib/async/default.h>

#include <algorithm>
#include <memory>

#include "base/fuchsia/fidl_event_handler.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace {

using fuchsia_accessibility_semantics::Node;

class AXFuchsiaSemanticProviderDelegate
    : public AXFuchsiaSemanticProvider::Delegate {
 public:
  AXFuchsiaSemanticProviderDelegate() = default;
  ~AXFuchsiaSemanticProviderDelegate() override = default;

  bool OnSemanticsManagerConnectionClosed(zx_status_t status) override {
    on_semantics_manager_connection_closed_called_ = true;
    return true;
  }

  bool OnAccessibilityAction(
      uint32_t node_id,
      fuchsia_accessibility_semantics::Action action) override {
    on_accessibility_action_called_ = true;
    on_accessibility_action_node_id_ = node_id;
    on_accessibility_action_action_ = std::move(action);
    return true;
  }

  void OnHitTest(fuchsia_math::PointF point,
                 HitTestCallback callback) override {
    on_hit_test_called_ = true;
    on_hit_test_point_ = std::move(point);
    std::move(callback).Run({});
  }

  void OnSemanticsEnabled(bool enabled) override {
    on_semantics_enabled_called_ = true;
  }

  bool on_semantics_manager_connection_closed_called_;
  bool on_accessibility_action_called_;
  uint32_t on_accessibility_action_node_id_ = 10000000;
  fuchsia_accessibility_semantics::Action on_accessibility_action_action_;
  bool on_hit_test_called_;
  fuchsia_math::PointF on_hit_test_point_;
  bool on_semantics_enabled_called_;
};

// Returns a semantic tree of the form:
// (0 (1 2 (3 4 (5))))
std::vector<Node> TreeNodes() {
  Node node_0;
  node_0.node_id(0u);
  node_0.child_ids({{1u, 2u}});

  Node node_1;
  node_1.node_id(1u);

  Node node_2;
  node_2.node_id(2u);
  node_2.child_ids({{3u, 4u}});

  Node node_3;
  node_3.node_id(3u);

  Node node_4;
  node_4.node_id(4u);
  node_4.child_ids({{5u}});

  Node node_5;
  node_5.node_id(5u);

  std::vector<Node> update;
  update.push_back(std::move(node_0));
  update.push_back(std::move(node_1));
  update.push_back(std::move(node_2));
  update.push_back(std::move(node_3));
  update.push_back(std::move(node_4));
  update.push_back(std::move(node_5));
  return update;
}

class AXFuchsiaSemanticProviderTest
    : public ::testing::Test,
      public fidl::Server<fuchsia_accessibility_semantics::SemanticsManager>,
      public fidl::Server<fuchsia_accessibility_semantics::SemanticTree> {
 public:
  AXFuchsiaSemanticProviderTest()
      : semantics_manager_bindings_(test_context_.additional_services(), this),
        semantic_listener_error_handler_(
            base::BindRepeating([](fidl::UnbindInfo info) { ADD_FAILURE(); })) {
  }
  ~AXFuchsiaSemanticProviderTest() override = default;
  AXFuchsiaSemanticProviderTest(const AXFuchsiaSemanticProviderTest&) = delete;
  AXFuchsiaSemanticProviderTest& operator=(
      const AXFuchsiaSemanticProviderTest&) = delete;
  void SetUp() override {
    fuchsia::ui::views::ViewRefControl view_ref_control;
    fuchsia::ui::views::ViewRef view_ref;
    auto status = zx::eventpair::create(
        /*options*/ 0u, &view_ref_control.reference, &view_ref.reference);
    CHECK_EQ(ZX_OK, status);
    view_ref.reference.replace(ZX_RIGHTS_BASIC, &view_ref.reference);

    delegate_ = std::make_unique<AXFuchsiaSemanticProviderDelegate>();

    semantic_provider_ = std::make_unique<AXFuchsiaSemanticProviderImpl>(
        fidl::HLCPPToNatural(std::move(view_ref)), delegate_.get());

    // Spin the loop to allow registration with the SemanticsManager to be
    // processed.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  // fuchsia_accessibility_semantics::SemanticsManager implementation.
  void RegisterViewForSemantics(
      AXFuchsiaSemanticProviderTest::RegisterViewForSemanticsRequest& request,
      AXFuchsiaSemanticProviderTest::RegisterViewForSemanticsCompleter::Sync&
          completer) final {
    semantic_listener_.Bind(std::move(request.listener()),
                            async_get_default_dispatcher(),
                            &semantic_listener_error_handler_);
    semantic_tree_binding_.emplace(async_get_default_dispatcher(),
                                   std::move(request.semantic_tree_request()),
                                   this, fidl::kIgnoreBindingClosure);
    semantic_listener_->OnSemanticsModeChanged({{.updates_enabled = true}})
        .Then(
            [](fidl::Result<fuchsia_accessibility_semantics::SemanticListener::
                                OnSemanticsModeChanged>& result) {
              ASSERT_TRUE(result.is_ok());
            });
  }

  // fuchsia_accessibility_semantics::SemanticTree implementation.
  void UpdateSemanticNodes(
      AXFuchsiaSemanticProviderTest::UpdateSemanticNodesRequest& request,
      AXFuchsiaSemanticProviderTest::UpdateSemanticNodesCompleter::Sync&
          ignored_completer) final {
    num_update_semantic_nodes_called_++;
    node_updates_.push_back(std::move(request.nodes()));
  }
  void DeleteSemanticNodes(
      AXFuchsiaSemanticProviderTest::DeleteSemanticNodesRequest& request,
      AXFuchsiaSemanticProviderTest::DeleteSemanticNodesCompleter::Sync&
          ignored_completer) final {
    num_delete_semantic_nodes_called_++;
  }
  void CommitUpdates(
      AXFuchsiaSemanticProviderTest::CommitUpdatesCompleter::Sync& completer)
      final {
    completer.Reply();
  }
  void SendSemanticEvent(
      AXFuchsiaSemanticProviderTest::SendSemanticEventRequest& request,
      AXFuchsiaSemanticProviderTest::SendSemanticEventCompleter::Sync&
          completer) override {
    completer.Reply();
  }

  const std::vector<std::vector<fuchsia_accessibility_semantics::Node>>&
  node_updates() {
    return node_updates_;
  }

  // Required because of |test_context_|.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::TestComponentContextForProcess test_context_;
  // Binding to fake Semantics Manager Fuchsia service, implemented by this test
  // class.
  base::ScopedNaturalServiceBinding<
      fuchsia_accessibility_semantics::SemanticsManager>
      semantics_manager_bindings_;

  uint32_t num_update_semantic_nodes_called_ = 0;
  uint32_t num_delete_semantic_nodes_called_ = 0;

  base::RepeatingClosure on_commit_;

  fidl::Client<fuchsia_accessibility_semantics::SemanticListener>
      semantic_listener_;
  base::FidlErrorEventHandler<fuchsia_accessibility_semantics::SemanticListener>
      semantic_listener_error_handler_;
  std::optional<
      fidl::ServerBinding<fuchsia_accessibility_semantics::SemanticTree>>
      semantic_tree_binding_;
  std::unique_ptr<AXFuchsiaSemanticProviderDelegate> delegate_;
  std::unique_ptr<AXFuchsiaSemanticProviderImpl> semantic_provider_;

  // Node updates batched per API call to UpdateSemanticNodes().
  std::vector<std::vector<fuchsia_accessibility_semantics::Node>> node_updates_;
};

TEST_F(AXFuchsiaSemanticProviderTest, HandlesOnSemanticsConnectionClosed) {
  ASSERT_TRUE(semantic_tree_binding_.has_value());
  semantic_tree_binding_->Close(ZX_ERR_PEER_CLOSED);

  // Spin the loop to allow the channel-close to be handled.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate_->on_semantics_manager_connection_closed_called_);
}

TEST_F(AXFuchsiaSemanticProviderTest, HandlesOnAccessibilityAction) {
  bool action_handled = false;
  semantic_listener_
      ->OnAccessibilityActionRequested({{
          .node_id = 1u,
          .action = fuchsia_accessibility_semantics::Action::kDefault,
      }})
      .Then([&action_handled](
                fidl::Result<fuchsia_accessibility_semantics::SemanticListener::
                                 OnAccessibilityActionRequested>& result) {
        ASSERT_TRUE(result.is_ok());
        action_handled = result->handled();
      });

  // Spin the loop to handle the request, and receive the response.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(action_handled);
  EXPECT_TRUE(delegate_->on_accessibility_action_called_);
  EXPECT_EQ(delegate_->on_accessibility_action_node_id_, 1u);
  EXPECT_EQ(delegate_->on_accessibility_action_action_,
            fuchsia_accessibility_semantics::Action::kDefault);
}

TEST_F(AXFuchsiaSemanticProviderTest, HandlesOnHitTest) {
  semantic_provider_->SetPixelScale(2.f);

  // Note that the point is sent here and will be converted according to the
  // device scale used. Only then it gets sent to the handler, which receives
  // the value already with the proper scaling.
  semantic_listener_
      ->HitTest({{
          .local_point = {{.x = 4, .y = 6}},
      }})
      .Then([](fidl::Result<
                fuchsia_accessibility_semantics::SemanticListener::HitTest>&
                   result) { ASSERT_TRUE(result.is_ok()); });

  // Spin the loop to allow the call to be processed.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate_->on_hit_test_called_);
  EXPECT_EQ(delegate_->on_hit_test_point_.x(), 8.0);
  EXPECT_EQ(delegate_->on_hit_test_point_.y(), 12.0);
}

// Verify that the AXFuchsiaSemanticProviderImpl constructor triggered the call
// chain RegisterViewForSemantics -> OnSemanticsModeChanged ->
// OnSemanticsEnabled.
TEST_F(AXFuchsiaSemanticProviderTest, HandlesOnSemanticsEnabled) {
  EXPECT_TRUE(delegate_->on_semantics_enabled_called_);
}

TEST_F(AXFuchsiaSemanticProviderTest, SendsRootOnly) {
  Node root;
  root.node_id(0u);
  EXPECT_TRUE(semantic_provider_->Update(std::move(root)));

  // Spin the loop to process the update call.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, SendsNodesFromRootToLeaves) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, SendsNodesFromLeavesToRoot) {
  auto nodes = TreeNodes();
  std::reverse(nodes.begin(), nodes.end());
  for (auto& node : nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest,
       SendsNodesOnlyAfterParentNoLongerPointsToDeletedChild) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());

  // Deletes node 5, which is a child of 4.
  EXPECT_TRUE(semantic_provider_->Delete(5u));

  // Spin the loop to process the deletion call.
  base::RunLoop().RunUntilIdle();

  // Commit is pending, because the parent still points to the child.
  EXPECT_TRUE(semantic_provider_->HasPendingUpdates());

  Node node_4;
  node_4.node_id(4u);
  node_4.child_ids({});
  EXPECT_TRUE(semantic_provider_->Update(std::move(node_4)));

  // Spin the loop to process the node update.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 2u);
  EXPECT_EQ(num_delete_semantic_nodes_called_, 1u);

  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest,
       SendsNodesOnlyAfterDanglingChildIsDeleted) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());

  Node node_4;
  node_4.node_id(4u);
  node_4.child_ids({});  // This removes child 5.
  EXPECT_TRUE(semantic_provider_->Update(std::move(node_4)));

  // Spin the loop to process the update call.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(semantic_provider_->HasPendingUpdates());

  EXPECT_TRUE(semantic_provider_->Delete(5u));

  // Spin the loop to process the deletion.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 2u);
  EXPECT_EQ(num_delete_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, ReparentsNodeWithADeletion) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());

  // Deletes node 4 to reparent its child (5).
  EXPECT_TRUE(semantic_provider_->Delete(4u));

  // Spin the loop to process the deletion.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(semantic_provider_->HasPendingUpdates());

  // Add child 5 to another node.
  Node node_1;
  node_1.node_id(1u);
  node_1.child_ids({{5u}});
  EXPECT_TRUE(semantic_provider_->Update(std::move(node_1)));

  // Spin the loop to process the update.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(semantic_provider_->HasPendingUpdates());

  Node node_4;
  node_4.node_id(4u);
  node_4.child_ids({});
  EXPECT_TRUE(semantic_provider_->Update(std::move(node_4)));

  // Spin the loop to process the update.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 2u);
  EXPECT_EQ(num_delete_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, ReparentsNodeWithAnUpdate) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());

  // Add child 5 to another node. Note that 5 will have two parents, and the
  // commit must be held until it has only one.
  Node node_1;
  node_1.node_id(1u);
  node_1.child_ids({{5u}});
  EXPECT_TRUE(semantic_provider_->Update(std::move(node_1)));

  // Spin the loop to process the update.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(semantic_provider_->HasPendingUpdates());

  // Updates node 4 to no longer point to 5.
  Node node_4;
  node_4.node_id(4u);
  node_4.child_ids({});
  EXPECT_TRUE(semantic_provider_->Update(std::move(node_4)));

  // Spin the loop to process the update.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 2u);
  EXPECT_EQ(num_delete_semantic_nodes_called_, 0u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, ChangesRoot) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued updated calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());

  Node new_root;
  new_root.node_id(0u);
  new_root.child_ids({{1u, 2u}});
  EXPECT_TRUE(semantic_provider_->Update(std::move(new_root)));

  // Spin the loop to process the update.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 2u);
  EXPECT_EQ(num_delete_semantic_nodes_called_, 0u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, BatchesUpdates) {
  std::vector<Node> updates;
  for (uint32_t i = 0; i < 30; ++i) {
    Node node;
    node.node_id(i);
    node.child_ids({{i + 1}});
    updates.push_back(std::move(node));
  }
  updates.back().child_ids()->clear();

  for (auto& node : updates) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  // 30 nodes in batches of 16 (default value of maximum nodes per update call),
  // should result in two update calls to the semantics API.
  EXPECT_EQ(num_update_semantic_nodes_called_, 2u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, ClearsTree) {
  auto tree_nodes = TreeNodes();
  for (auto& node : tree_nodes) {
    EXPECT_TRUE(semantic_provider_->Update(std::move(node)));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());

  semantic_provider_->Clear();

  // Spin the loop to process the clear-tree call.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(num_update_semantic_nodes_called_, 1u);
  EXPECT_EQ(num_delete_semantic_nodes_called_, 1u);
  EXPECT_FALSE(semantic_provider_->HasPendingUpdates());
}

TEST_F(AXFuchsiaSemanticProviderTest, UpdateScaleFactor) {
  // Send an initial root node update. At this point, the pixel scale is 1, the
  // root node's transform will be the identity matrix. Thus, the resulting
  // update sent to fuchsia should not contain a transform.
  {
    Node node;
    node.node_id(0u);
    // Set child_ids to make sure they're not overwritten later.
    node.child_ids({{1u}});
    semantic_provider_->Update(std::move(node));
    Node child;
    child.node_id(1u);
    semantic_provider_->Update(std::move(child));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  // Check that the first update sent to fuchsia reflects a pixel scale of 1.
  {
    ASSERT_EQ(node_updates().size(), 1u);
    const auto& first_update_batch = node_updates()[0];
    ASSERT_EQ(first_update_batch.size(), 2u);
    EXPECT_EQ(first_update_batch[0].node_id(), 0u);
    const fuchsia_accessibility_semantics::Node& node = first_update_batch[0];
    ASSERT_TRUE(node.node_to_container_transform().has_value());
    const auto& transform = node.node_to_container_transform()->matrix();
    EXPECT_EQ(transform[0], 1.f);
    EXPECT_EQ(transform[5], 1.f);
    ASSERT_EQ(node.child_ids()->size(), 1u);
    EXPECT_EQ(node.child_ids().value()[0], 1u);
  }

  // Now, set a new pixel scale != 1. This step should force an update to
  // fuchsia.
  const auto kPixelScale = 0.5f;
  semantic_provider_->SetPixelScale(kPixelScale);

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  // Check that the root node's node_to_conatiner_transform field was set when
  // the pixel scale was updated.
  {
    ASSERT_EQ(node_updates().size(), 2u);
    const auto& second_update_batch = node_updates()[1];
    ASSERT_EQ(second_update_batch.size(), 1u);
    const fuchsia_accessibility_semantics::Node& node = second_update_batch[0];
    EXPECT_EQ(node.node_id(), 0u);
    ASSERT_TRUE(node.node_to_container_transform().has_value());
    const auto& transform = node.node_to_container_transform()->matrix();
    EXPECT_EQ(transform[0], 1.f / kPixelScale);
    EXPECT_EQ(transform[5], 1.f / kPixelScale);
    ASSERT_EQ(node.child_ids()->size(), 1u);
    EXPECT_EQ(node.child_ids().value()[0], 1u);
  }

  // Finally, send one more update, and verify that the semantic provider
  // accounted for the new pixel scale in the root node's transform.
  {
    Node node;
    node.node_id(0u);
    node.child_ids({{1u}});
    semantic_provider_->Update(std::move(node));
  }

  // Spin the loop to process the queued update calls.
  base::RunLoop().RunUntilIdle();

  // Check that the root node's node_to_conatiner_transform field was set using
  // the new pixel scale.
  {
    ASSERT_EQ(node_updates().size(), 3u);
    const auto& third_update_batch = node_updates()[2];
    ASSERT_EQ(third_update_batch.size(), 1u);
    const fuchsia_accessibility_semantics::Node& node = third_update_batch[0];
    EXPECT_EQ(node.node_id(), 0u);
    ASSERT_TRUE(node.node_to_container_transform().has_value());
    const auto& transform = node.node_to_container_transform()->matrix();
    EXPECT_EQ(transform[0], 1.f / kPixelScale);
    EXPECT_EQ(transform[5], 1.f / kPixelScale);
    ASSERT_EQ(node.child_ids()->size(), 1u);
    EXPECT_EQ(node.child_ids().value()[0], 1u);
  }
}

}  // namespace
}  // namespace ui
