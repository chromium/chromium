// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_impl.h"
#include "ui/accessibility/platform/fuchsia/ax_platform_node_fuchsia.h"
#include "ui/accessibility/platform/fuchsia/semantic_provider.h"

namespace ui {
namespace {

class MockSemanticProvider : public AXFuchsiaSemanticProvider {
 public:
  // AXFuchsiaSemanticProvider overrides.
  bool Update(fuchsia::accessibility::semantics::Node node) override {
    last_update_ = std::move(node);
    return true;
  }

  bool Delete(uint32_t node_id) override {
    last_deletion_ = node_id;
    return true;
  }

  bool Clear() override { return true; }

  void SendEvent(
      fuchsia::accessibility::semantics::SemanticEvent event) override {
    last_event_ = std::move(event);
  }

  bool HasPendingUpdates() const override { return false; }

  float GetPixelScale() const override { return pixel_scale_; }

  void SetPixelScale(float pixel_scale) override { pixel_scale_ = pixel_scale; }

  const absl::optional<fuchsia::accessibility::semantics::Node>& last_update()
      const {
    return last_update_;
  }
  const absl::optional<uint32_t>& last_deletion() const {
    return last_deletion_;
  }
  const absl::optional<fuchsia::accessibility::semantics::SemanticEvent>&
  last_event() const {
    return last_event_;
  }

 private:
  absl::optional<fuchsia::accessibility::semantics::Node> last_update_;
  absl::optional<uint32_t> last_deletion_;
  absl::optional<fuchsia::accessibility::semantics::SemanticEvent> last_event_;
  float pixel_scale_ = 1.f;
};

class MockAXPlatformNodeDelegate : public AXPlatformNodeDelegate {
 public:
  MockAXPlatformNodeDelegate() = default;
  ~MockAXPlatformNodeDelegate() override = default;

  bool AccessibilityPerformAction(const AXActionData& data) override {
    last_action_data_.emplace(data);
    return true;
  }

  const AXUniqueId& GetUniqueId() const override { return unique_id_; }

  const absl::optional<AXActionData>& last_action_data() {
    return last_action_data_;
  }

  const ui::AXNodeData& GetData() const override { return ax_node_data_; }
  void SetData(ui::AXNodeData ax_node_data) { ax_node_data_ = ax_node_data; }

 private:
  absl::optional<AXActionData> last_action_data_;
  ui::AXUniqueId unique_id_;
  ui::AXNodeData ax_node_data_ = {};
};

class AccessibilityBridgeFuchsiaTest : public ::testing::Test {
 public:
  AccessibilityBridgeFuchsiaTest() = default;
  ~AccessibilityBridgeFuchsiaTest() override = default;

  void SetUp() override {
    mock_ax_platform_node_delegate_ =
        std::make_unique<MockAXPlatformNodeDelegate>();
    auto mock_semantic_provider = std::make_unique<MockSemanticProvider>();
    mock_semantic_provider_ = mock_semantic_provider.get();
    auto view_ref_pair = scenic::ViewRefPair::New();
    accessibility_bridge_ = std::make_unique<AccessibilityBridgeFuchsiaImpl>(
        /*root_window=*/nullptr, std::move(view_ref_pair.view_ref),
        base::RepeatingCallback<void(bool)>(),
        base::RepeatingCallback<bool(zx_status_t)>(), inspect::Node());
    accessibility_bridge_->set_semantic_provider_for_test(
        std::move(mock_semantic_provider));
  }

 protected:
  // Required for scenic::ViewRefPair::New().
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  std::unique_ptr<MockAXPlatformNodeDelegate> mock_ax_platform_node_delegate_;
  MockSemanticProvider* mock_semantic_provider_;
  std::unique_ptr<AccessibilityBridgeFuchsiaImpl> accessibility_bridge_;
};

TEST_F(AccessibilityBridgeFuchsiaTest, UpdateNode) {
  fuchsia::accessibility::semantics::Node node;
  node.set_node_id(1u);
  node.mutable_attributes()->set_label("label");

  accessibility_bridge_->UpdateNode(std::move(node));

  const absl::optional<fuchsia::accessibility::semantics::Node>& last_update =
      mock_semantic_provider_->last_update();
  ASSERT_TRUE(last_update.has_value());
  EXPECT_EQ(last_update->node_id(), 1u);
  ASSERT_TRUE(last_update->has_attributes());
  ASSERT_TRUE(last_update->attributes().has_label());
  EXPECT_EQ(last_update->attributes().label(), "label");
}

TEST_F(AccessibilityBridgeFuchsiaTest, UpdateNodeReplaceNodeID) {
  accessibility_bridge_->SetRootID(1u);

  fuchsia::accessibility::semantics::Node node;
  node.set_node_id(1u);

  accessibility_bridge_->UpdateNode(std::move(node));

  const absl::optional<fuchsia::accessibility::semantics::Node>& last_update =
      mock_semantic_provider_->last_update();
  ASSERT_TRUE(last_update.has_value());
  EXPECT_EQ(last_update->node_id(),
            AXFuchsiaSemanticProvider::kFuchsiaRootNodeId);
}

TEST_F(AccessibilityBridgeFuchsiaTest, UpdateNodeReplaceOffsetContainerID) {
  accessibility_bridge_->SetRootID(1u);

  fuchsia::accessibility::semantics::Node node;
  node.set_node_id(2u);
  node.set_container_id(1u);

  accessibility_bridge_->UpdateNode(std::move(node));

  const absl::optional<fuchsia::accessibility::semantics::Node>& last_update =
      mock_semantic_provider_->last_update();
  ASSERT_TRUE(last_update.has_value());
  ASSERT_TRUE(last_update->has_container_id());
  EXPECT_EQ(last_update->container_id(),
            AXFuchsiaSemanticProvider::kFuchsiaRootNodeId);
}

TEST_F(AccessibilityBridgeFuchsiaTest, SetRootIDDeletesOldRoot) {
  accessibility_bridge_->SetRootID(1u);
  accessibility_bridge_->SetRootID(2u);

  const absl::optional<uint32_t>& last_deletion =
      mock_semantic_provider_->last_deletion();
  ASSERT_TRUE(last_deletion.has_value());
  EXPECT_EQ(*last_deletion, AXFuchsiaSemanticProvider::kFuchsiaRootNodeId);
}

TEST_F(AccessibilityBridgeFuchsiaTest, DeleteNode) {
  accessibility_bridge_->SetRootID(1u);

  // Delete a non-root node.
  accessibility_bridge_->DeleteNode(2u);

  const absl::optional<uint32_t>& last_deletion =
      mock_semantic_provider_->last_deletion();
  ASSERT_TRUE(last_deletion.has_value());
  EXPECT_EQ(*last_deletion, 2u);
}

TEST_F(AccessibilityBridgeFuchsiaTest, DeleteRoot) {
  accessibility_bridge_->SetRootID(1u);

  // Delete root node.
  accessibility_bridge_->DeleteNode(1u);

  {
    const absl::optional<uint32_t>& last_deletion =
        mock_semantic_provider_->last_deletion();
    ASSERT_TRUE(last_deletion.has_value());
    EXPECT_EQ(*last_deletion, AXFuchsiaSemanticProvider::kFuchsiaRootNodeId);
  }

  // Delete the node ID 1 again, and verify that it's no longer mapped to the
  // root.
  accessibility_bridge_->DeleteNode(1u);

  {
    const absl::optional<uint32_t>& last_deletion =
        mock_semantic_provider_->last_deletion();
    ASSERT_TRUE(last_deletion.has_value());
    EXPECT_EQ(*last_deletion, 1u);
  }
}

TEST_F(AccessibilityBridgeFuchsiaTest, HitTest) {
  auto root_delegate = std::make_unique<MockAXPlatformNodeDelegate>();
  AXPlatformNode* root_platform_node =
      AXPlatformNode::Create(root_delegate.get());

  auto child_delegate = std::make_unique<MockAXPlatformNodeDelegate>();
  AXPlatformNode* child_platform_node =
      AXPlatformNode::Create(child_delegate.get());

  // Set the platform node as the root, so that the accessibility bridge
  // dispatches the hit test request to it.
  accessibility_bridge_->SetRootID(root_platform_node->GetUniqueId());

  fuchsia::math::PointF target_point;
  target_point.x = 1.f;
  target_point.y = 2.f;

  // Set hit_test_result to a nonsense value to ensure that it's modified later.
  uint32_t hit_test_result = 100u;

  // Request a hit test. Note that the callback will not be invoked until
  // OnAccessibilityHitTestResult() is called.
  accessibility_bridge_->OnHitTest(
      target_point,
      [&hit_test_result](fuchsia::accessibility::semantics::Hit hit) {
        ASSERT_TRUE(hit.has_node_id());
        hit_test_result = hit.node_id();
      });

  // Verify that the platform node's delegate received the hit test request.
  const absl::optional<ui::AXActionData>& action_data =
      root_delegate->last_action_data();
  ASSERT_TRUE(action_data.has_value());

  // The request_id field defaults to -1, so verify that it was set to a
  // non-negative value.
  EXPECT_GE(action_data->request_id, 0);
  EXPECT_EQ(action_data->target_point.x(), target_point.x);
  EXPECT_EQ(action_data->target_point.y(), target_point.y);
  EXPECT_EQ(action_data->action, ax::mojom::Action::kHitTest);

  // Simulate a hit test result. This should invoke the callback.
  uint32_t child_node_id =
      static_cast<uint32_t>(child_platform_node->GetUniqueId());
  accessibility_bridge_->OnAccessibilityHitTestResult(
      action_data->request_id, absl::optional<uint32_t>(child_node_id));

  EXPECT_EQ(hit_test_result, static_cast<uint32_t>(child_node_id));
}

TEST_F(AccessibilityBridgeFuchsiaTest, HitTestReturnsRoot) {
  auto root_delegate = std::make_unique<MockAXPlatformNodeDelegate>();
  AXPlatformNode* root_platform_node =
      AXPlatformNode::Create(root_delegate.get());

  // Set the platform node as the root, so that the accessibility bridge
  // dispatches the hit test request to it.
  accessibility_bridge_->SetRootID(root_platform_node->GetUniqueId());

  fuchsia::math::PointF target_point;
  target_point.x = 1.f;
  target_point.y = 2.f;

  // Set hit_test_result to a nonsense value to ensure that it's modified later.
  uint32_t hit_test_result = 100u;

  // Request a hit test. Note that the callback will not be invoked until
  // OnAccessibilityHitTestResult() is called.
  accessibility_bridge_->OnHitTest(
      target_point,
      [&hit_test_result](fuchsia::accessibility::semantics::Hit hit) {
        ASSERT_TRUE(hit.has_node_id());
        hit_test_result = hit.node_id();
      });

  const absl::optional<ui::AXActionData>& action_data =
      root_delegate->last_action_data();
  ASSERT_TRUE(action_data.has_value());

  // Simulate a hit test result. This should invoke the callback.
  uint32_t root_node_id =
      static_cast<uint32_t>(root_platform_node->GetUniqueId());
  accessibility_bridge_->OnAccessibilityHitTestResult(
      action_data->request_id, absl::optional<uint32_t>(root_node_id));

  EXPECT_EQ(hit_test_result, AXFuchsiaSemanticProvider::kFuchsiaRootNodeId);
}

TEST_F(AccessibilityBridgeFuchsiaTest, HitTestReturnsEmptyResult) {
  auto root_delegate = std::make_unique<MockAXPlatformNodeDelegate>();
  AXPlatformNode* root_platform_node =
      AXPlatformNode::Create(root_delegate.get());

  // Set the platform node as the root, so that the accessibility bridge
  // dispatches the hit test request to it.
  accessibility_bridge_->SetRootID(root_platform_node->GetUniqueId());

  fuchsia::math::PointF target_point;
  target_point.x = 1.f;
  target_point.y = 2.f;

  // Request a hit test. Note that the callback will not be invoked until
  // OnAccessibilityHitTestResult() is called.
  bool callback_ran = false;
  accessibility_bridge_->OnHitTest(
      target_point,
      [&callback_ran](fuchsia::accessibility::semantics::Hit hit) {
        callback_ran = true;
        ASSERT_FALSE(hit.has_node_id());
      });

  const absl::optional<ui::AXActionData>& action_data =
      root_delegate->last_action_data();
  ASSERT_TRUE(action_data.has_value());

  // Simulate a hit test result. This should invoke the callback.
  accessibility_bridge_->OnAccessibilityHitTestResult(action_data->request_id,
                                                      {});

  // Verify that the callback ran. The callback itself will check that the hit
  // result was empty.
  EXPECT_TRUE(callback_ran);
}

TEST_F(AccessibilityBridgeFuchsiaTest, PerformActionOnRoot) {
  auto root_delegate = std::make_unique<MockAXPlatformNodeDelegate>();
  AXPlatformNode* root_platform_node =
      AXPlatformNode::Create(root_delegate.get());

  // Set the platform node as the root, so that the accessibility bridge
  // dispatches the hit test request to it.
  accessibility_bridge_->SetRootID(root_platform_node->GetUniqueId());

  // Perform DEFAULT action.
  accessibility_bridge_->OnAccessibilityAction(
      AXFuchsiaSemanticProvider::kFuchsiaRootNodeId,
      fuchsia::accessibility::semantics::Action::DEFAULT);

  const absl::optional<ui::AXActionData>& action_data =
      root_delegate->last_action_data();
  ASSERT_TRUE(action_data.has_value());
  EXPECT_EQ(action_data->action, ax::mojom::Action::kDoDefault);
}

TEST_F(AccessibilityBridgeFuchsiaTest, ScrollToMakeVisible) {
  auto delegate = std::make_unique<MockAXPlatformNodeDelegate>();
  ui::AXNodeData data;
  data.relative_bounds.bounds = gfx::RectF(
      /*x_min=*/1.f, /*y_min=*/2.f, /*width=*/3.f, /*height=*/4.f);
  delegate->SetData(data);
  AXPlatformNode* platform_node = AXPlatformNode::Create(delegate.get());
  ASSERT_TRUE(static_cast<AXPlatformNodeFuchsia*>(platform_node));

  // Request a SHOW_ON_SCREEN action.
  accessibility_bridge_->OnAccessibilityAction(
      delegate->GetUniqueId(),
      fuchsia::accessibility::semantics::Action::SHOW_ON_SCREEN);

  const absl::optional<ui::AXActionData>& action_data =
      delegate->last_action_data();
  ASSERT_TRUE(action_data.has_value());
  EXPECT_EQ(action_data->action, ax::mojom::Action::kScrollToMakeVisible);

  // The target rect should have the same size as the node's bounds, but should
  // have (x, y) == (0, 0).
  EXPECT_EQ(action_data->target_rect.x(), 0.f);
  EXPECT_EQ(action_data->target_rect.y(), 0.f);
  EXPECT_EQ(action_data->target_rect.width(), 3.f);
  EXPECT_EQ(action_data->target_rect.height(), 4.f);
}

}  // namespace
}  // namespace ui
