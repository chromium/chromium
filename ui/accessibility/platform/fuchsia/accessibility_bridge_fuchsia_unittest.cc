// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fidl/fuchsia.accessibility.semantics/cpp/fidl.h>
#include <fidl/fuchsia.ui.views/cpp/hlcpp_conversion.h>
#include <lib/zx/eventpair.h>

#include <optional>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_impl.h"
#include "ui/accessibility/platform/fuchsia/ax_platform_node_fuchsia.h"
#include "ui/accessibility/platform/fuchsia/semantic_provider.h"

namespace ui {
namespace {

class FakeSemanticProvider : public AXFuchsiaSemanticProvider {
 public:
  // AXFuchsiaSemanticProvider overrides.
  bool Update(fuchsia_accessibility_semantics::Node node) override {
    last_update_ = std::move(node);
    return true;
  }

  bool Delete(uint32_t node_id) override {
    last_deletion_ = node_id;
    return true;
  }

  bool Clear() override { return true; }

  void SendEvent(
      fuchsia_accessibility_semantics::SemanticEvent event) override {
    last_event_ = std::move(event);
  }

  bool HasPendingUpdates() const override { return false; }

  float GetPixelScale() const override { return pixel_scale_; }

  void SetPixelScale(float pixel_scale) override { pixel_scale_ = pixel_scale; }

  const std::optional<fuchsia_accessibility_semantics::Node>& last_update()
      const {
    return last_update_;
  }
  const std::optional<uint32_t>& last_deletion() const {
    return last_deletion_;
  }
  const std::optional<fuchsia_accessibility_semantics::SemanticEvent>&
  last_event() const {
    return last_event_;
  }

 private:
  std::optional<fuchsia_accessibility_semantics::Node> last_update_;
  std::optional<uint32_t> last_deletion_;
  std::optional<fuchsia_accessibility_semantics::SemanticEvent> last_event_;
  float pixel_scale_ = 1.f;
};

class FakeAXPlatformNodeDelegate : public AXPlatformNodeDelegate {
 public:
  FakeAXPlatformNodeDelegate() = default;
  ~FakeAXPlatformNodeDelegate() override = default;

  bool AccessibilityPerformAction(const AXActionData& data) override {
    last_action_data_.emplace(data);
    return true;
  }

  AXPlatformNodeId GetUniqueId() const override { return unique_id_; }

  const std::optional<AXActionData>& last_action_data() {
    return last_action_data_;
  }

  const AXNodeData& GetData() const override { return ax_node_data_; }
  void SetData(AXNodeData ax_node_data) { ax_node_data_ = ax_node_data; }

 private:
  std::optional<AXActionData> last_action_data_;
  const AXUniqueId unique_id_{AXUniqueId::Create()};
  AXNodeData ax_node_data_ = {};
};

class AccessibilityBridgeFuchsiaTest : public ::testing::Test {
 public:
  AccessibilityBridgeFuchsiaTest() = default;
  ~AccessibilityBridgeFuchsiaTest() override = default;

  void SetUp() override {
    mock_ax_platform_node_delegate_ =
        std::make_unique<FakeAXPlatformNodeDelegate>();
    auto mock_semantic_provider = std::make_unique<FakeSemanticProvider>();
    mock_semantic_provider_ = mock_semantic_provider.get();

    fuchsia::ui::views::ViewRefControl view_ref_control;
    fuchsia::ui::views::ViewRef view_ref;
    auto status = zx::eventpair::create(
        /*options*/ 0u, &view_ref_control.reference, &view_ref.reference);
    CHECK_EQ(ZX_OK, status);
    view_ref.reference.replace(ZX_RIGHTS_BASIC, &view_ref.reference);

    accessibility_bridge_ = std::make_unique<AccessibilityBridgeFuchsiaImpl>(
        /*root_window=*/nullptr, fidl::HLCPPToNatural(std::move(view_ref)),
        base::RepeatingCallback<void(bool)>(),
        base::RepeatingCallback<bool(zx_status_t)>(), inspect::Node());
    accessibility_bridge_->set_semantic_provider_for_test(
        std::move(mock_semantic_provider));
  }

 protected:
  // Required for zx::eventpair::create.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  std::unique_ptr<FakeAXPlatformNodeDelegate> mock_ax_platform_node_delegate_;
  FakeSemanticProvider* mock_semantic_provider_;
  std::unique_ptr<AccessibilityBridgeFuchsiaImpl> accessibility_bridge_;
};

TEST_F(AccessibilityBridgeFuchsiaTest, UpdateNode) {
  accessibility_bridge_->UpdateNode({{
      .node_id = 1u,
      .attributes = fuchsia_accessibility_semantics::Attributes{{
          .label = "label",
      }},
  }});

  const std::optional<fuchsia_accessibility_semantics::Node>& last_update =
      mock_semantic_provider_->last_update();

  ASSERT_TRUE(last_update.has_value());
  EXPECT_EQ(last_update->node_id(), 1u);
  ASSERT_TRUE(last_update->attributes());
  ASSERT_TRUE(last_update->attributes()->label().has_value());
  EXPECT_EQ(last_update->attributes()->label().value(), "label");
}

TEST_F(AccessibilityBridgeFuchsiaTest, UpdateNodeReplaceNodeID) {
  accessibility_bridge_->SetRootID(1u);

  fuchsia_accessibility_semantics::Node node;
  node.node_id(1u);

  accessibility_bridge_->UpdateNode(std::move(node));

  const std::optional<fuchsia_accessibility_semantics::Node>& last_update =
      mock_semantic_provider_->last_update();
  ASSERT_TRUE(last_update.has_value());
  EXPECT_EQ(last_update->node_id(),
            AXFuchsiaSemanticProvider::kFuchsiaRootNodeId);
}

TEST_F(AccessibilityBridgeFuchsiaTest, UpdateNodeReplaceOffsetContainerID) {
  accessibility_bridge_->SetRootID(1u);

  fuchsia_accessibility_semantics::Node node;
  node.node_id(2u);
  node.container_id(1u);

  accessibility_bridge_->UpdateNode(std::move(node));

  const std::optional<fuchsia_accessibility_semantics::Node>& last_update =
      mock_semantic_provider_->last_update();
  ASSERT_TRUE(last_update.has_value());
  ASSERT_TRUE(last_update->container_id().has_value());
  EXPECT_EQ(last_update->container_id().value(),
            AXFuchsiaSemanticProvider::kFuchsiaRootNodeId);
}

TEST_F(AccessibilityBridgeFuchsiaTest, SetRootIDDeletesOldRoot) {
  accessibility_bridge_->SetRootID(1u);
  accessibility_bridge_->SetRootID(2u);

  const std::optional<uint32_t>& last_deletion =
      mock_semantic_provider_->last_deletion();
  ASSERT_TRUE(last_deletion.has_value());
  EXPECT_EQ(*last_deletion, AXFuchsiaSemanticProvider::kFuchsiaRootNodeId);
}

TEST_F(AccessibilityBridgeFuchsiaTest, DeleteNode) {
  accessibility_bridge_->SetRootID(1u);

  // Delete a non-root node.
  accessibility_bridge_->DeleteNode(2u);

  const std::optional<uint32_t>& last_deletion =
      mock_semantic_provider_->last_deletion();
  ASSERT_TRUE(last_deletion.has_value());
  EXPECT_EQ(*last_deletion, 2u);
}

TEST_F(AccessibilityBridgeFuchsiaTest, DeleteRoot) {
  accessibility_bridge_->SetRootID(1u);

  // Delete root node.
  accessibility_bridge_->DeleteNode(1u);

  {
    const std::optional<uint32_t>& last_deletion =
        mock_semantic_provider_->last_deletion();
    ASSERT_TRUE(last_deletion.has_value());
    EXPECT_EQ(*last_deletion, AXFuchsiaSemanticProvider::kFuchsiaRootNodeId);
  }

  // Delete the node ID 1 again, and verify that it's no longer mapped to the
  // root.
  accessibility_bridge_->DeleteNode(1u);

  {
    const std::optional<uint32_t>& last_deletion =
        mock_semantic_provider_->last_deletion();
    ASSERT_TRUE(last_deletion.has_value());
    EXPECT_EQ(*last_deletion, 1u);
  }
}

TEST_F(AccessibilityBridgeFuchsiaTest, HitTest) {
  auto root_delegate = std::make_unique<FakeAXPlatformNodeDelegate>();
  AXPlatformNode* root_platform_node =
      AXPlatformNode::Create(root_delegate.get());

  auto child_delegate = std::make_unique<FakeAXPlatformNodeDelegate>();
  AXPlatformNode* child_platform_node =
      AXPlatformNode::Create(child_delegate.get());

  // Set the platform node as the root, so that the accessibility bridge
  // dispatches the hit test request to it.
  accessibility_bridge_->SetRootID(root_platform_node->GetUniqueId());

  fuchsia_math::PointF target_point = {{
      .x = 1.f,
      .y = 2.f,
  }};

  // Set hit_test_result to a nonsense value to ensure that it's modified
  // later.
  uint32_t hit_test_result = 100u;

  // Request a hit test. Note that the callback will not be invoked until
  // OnAccessibilityHitTestResult() is called.
  accessibility_bridge_->OnHitTest(
      target_point,
      base::BindLambdaForTesting(
          [&hit_test_result](
              const fidl::Response<
                  fuchsia_accessibility_semantics::SemanticListener::HitTest>&
                  response) {
            ASSERT_TRUE(response.result().node_id().has_value());
            hit_test_result = response.result().node_id().value();
          }));

  // Verify that the platform node's delegate received the hit test request.
  const std::optional<AXActionData>& action_data =
      root_delegate->last_action_data();
  ASSERT_TRUE(action_data.has_value());

  // The request_id field defaults to -1, so verify that it was set to a
  // non-negative value.
  EXPECT_GE(action_data->request_id, 0);
  EXPECT_EQ(action_data->target_point.x(), target_point.x());
  EXPECT_EQ(action_data->target_point.y(), target_point.y());
  EXPECT_EQ(action_data->action, ax::mojom::Action::kHitTest);

  // Simulate a hit test result. This should invoke the callback.
  uint32_t child_node_id =
      static_cast<uint32_t>(child_platform_node->GetUniqueId());
  accessibility_bridge_->OnAccessibilityHitTestResult(
      action_data->request_id, std::optional<uint32_t>(child_node_id));

  EXPECT_EQ(hit_test_result, static_cast<uint32_t>(child_node_id));
}

TEST_F(AccessibilityBridgeFuchsiaTest, HitTestReturnsRoot) {
  auto root_delegate = std::make_unique<FakeAXPlatformNodeDelegate>();
  AXPlatformNode* root_platform_node =
      AXPlatformNode::Create(root_delegate.get());

  // Set the platform node as the root, so that the accessibility bridge
  // dispatches the hit test request to it.
  accessibility_bridge_->SetRootID(root_platform_node->GetUniqueId());

  fuchsia_math::PointF target_point = {{
      .x = 1.f,
      .y = 2.f,
  }};

  // Set hit_test_result to a nonsense value to ensure that it's modified
  // later.
  uint32_t hit_test_result = 100u;

  // Request a hit test. Note that the callback will not be invoked until
  // OnAccessibilityHitTestResult() is called.
  accessibility_bridge_->OnHitTest(
      target_point,
      base::BindLambdaForTesting(
          [&hit_test_result](
              const fidl::Response<
                  fuchsia_accessibility_semantics::SemanticListener::HitTest>&
                  response) {
            ASSERT_TRUE(response.result().node_id().has_value());
            hit_test_result = response.result().node_id().value();
          }));

  const std::optional<AXActionData>& action_data =
      root_delegate->last_action_data();
  ASSERT_TRUE(action_data.has_value());

  // Simulate a hit test result. This should invoke the callback.
  uint32_t root_node_id =
      static_cast<uint32_t>(root_platform_node->GetUniqueId());
  accessibility_bridge_->OnAccessibilityHitTestResult(
      action_data->request_id, std::optional<uint32_t>(root_node_id));

  EXPECT_EQ(hit_test_result, AXFuchsiaSemanticProvider::kFuchsiaRootNodeId);
}

TEST_F(AccessibilityBridgeFuchsiaTest, HitTestReturnsEmptyResult) {
  auto root_delegate = std::make_unique<FakeAXPlatformNodeDelegate>();
  AXPlatformNode* root_platform_node =
      AXPlatformNode::Create(root_delegate.get());

  // Set the platform node as the root, so that the accessibility bridge
  // dispatches the hit test request to it.
  accessibility_bridge_->SetRootID(root_platform_node->GetUniqueId());

  fuchsia_math::PointF target_point{{
      .x = 1.f,
      .y = 2.f,
  }};

  // Request a hit test. Note that the callback will not be invoked until
  // OnAccessibilityHitTestResult() is called.
  bool callback_ran = false;
  accessibility_bridge_->OnHitTest(
      target_point,
      base::BindLambdaForTesting(
          [&callback_ran](
              const fidl::Response<
                  fuchsia_accessibility_semantics::SemanticListener::HitTest>&
                  response) {
            callback_ran = true;
            ASSERT_FALSE(response.result().node_id().has_value());
          }));

  const std::optional<AXActionData>& action_data =
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
  auto root_delegate = std::make_unique<FakeAXPlatformNodeDelegate>();
  AXPlatformNode* root_platform_node =
      AXPlatformNode::Create(root_delegate.get());

  // Set the platform node as the root, so that the accessibility bridge
  // dispatches the hit test request to it.
  accessibility_bridge_->SetRootID(root_platform_node->GetUniqueId());

  // Perform DEFAULT action.
  accessibility_bridge_->OnAccessibilityAction(
      AXFuchsiaSemanticProvider::kFuchsiaRootNodeId,
      fuchsia_accessibility_semantics::Action::kDefault);

  const std::optional<AXActionData>& action_data =
      root_delegate->last_action_data();
  ASSERT_TRUE(action_data.has_value());
  EXPECT_EQ(action_data->action, ax::mojom::Action::kDoDefault);
}

TEST_F(AccessibilityBridgeFuchsiaTest, ScrollToMakeVisible) {
  auto delegate = std::make_unique<FakeAXPlatformNodeDelegate>();
  AXNodeData data;
  data.relative_bounds.bounds = gfx::RectF(
      /*x_min=*/1.f, /*y_min=*/2.f, /*width=*/3.f, /*height=*/4.f);
  delegate->SetData(data);
  AXPlatformNode* platform_node = AXPlatformNode::Create(delegate.get());
  ASSERT_TRUE(static_cast<AXPlatformNodeFuchsia*>(platform_node));

  // Request a SHOW_ON_SCREEN action.
  accessibility_bridge_->OnAccessibilityAction(
      delegate->GetUniqueId(),
      fuchsia_accessibility_semantics::Action::kShowOnScreen);

  const std::optional<AXActionData>& action_data = delegate->last_action_data();
  ASSERT_TRUE(action_data.has_value());
  EXPECT_EQ(action_data->action, ax::mojom::Action::kScrollToMakeVisible);

  // The target rect should have the same size as the node's bounds, but
  // should have (x, y) == (0, 0).
  EXPECT_EQ(action_data->target_rect.x(), 0.f);
  EXPECT_EQ(action_data->target_rect.y(), 0.f);
  EXPECT_EQ(action_data->target_rect.width(), 3.f);
  EXPECT_EQ(action_data->target_rect.height(), 4.f);
}

}  // namespace
}  // namespace ui
