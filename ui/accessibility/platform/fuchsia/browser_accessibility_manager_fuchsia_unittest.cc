// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/fuchsia/browser_accessibility_manager_fuchsia.h"

#include <map>
#include <vector>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/accessibility/platform/ax_platform_tree_manager_delegate.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/fuchsia/browser_accessibility_fuchsia.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia.h"
#include "ui/accessibility/platform/fuchsia/accessibility_bridge_fuchsia_registry.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"
#include "ui/accessibility/platform/test_ax_platform_tree_manager_delegate.h"

namespace ui {
namespace {

class MockBrowserAccessibilityDelegate
    : public TestAXPlatformTreeManagerDelegate {
 public:
  void AccessibilityPerformAction(const AXActionData& data) override {
    last_action_data_ = data;
  }

  void AccessibilityHitTest(
      const gfx::Point& point_in_frame_pixels,
      const ax::mojom::Event& opt_event_to_fire,
      int opt_request_id,
      base::OnceCallback<void(AXPlatformTreeManager* hit_manager,
                              AXNodeID hit_node_id)> opt_callback) override {
    last_hit_test_point_ = point_in_frame_pixels;
    last_request_id_ = opt_request_id;
  }

  const std::optional<AXActionData>& last_action_data() {
    return last_action_data_;
  }

  const std::optional<int>& last_request_id() { return last_request_id_; }
  const std::optional<gfx::Point>& last_hit_test_point() {
    return last_hit_test_point_;
  }

 private:
  std::optional<AXActionData> last_action_data_;
  std::optional<int> last_request_id_;
  std::optional<gfx::Point> last_hit_test_point_;
};

class MockAccessibilityBridge : public AccessibilityBridgeFuchsia {
 public:
  MockAccessibilityBridge() {
    BrowserAccessibilityManagerFuchsia::SetAccessibilityBridgeForTest(this);
  }
  ~MockAccessibilityBridge() override {
    BrowserAccessibilityManagerFuchsia::SetAccessibilityBridgeForTest(nullptr);
  }

  // AccessibilityBridgeFuchsia overrides.
  void UpdateNode(fuchsia_accessibility_semantics::Node node) override {
    node_updates_.push_back(std::move(node));
  }

  void DeleteNode(uint32_t node_id) override {
    node_deletions_.push_back(node_id);
  }

  void OnAccessibilityHitTestResult(int hit_test_request_id,
                                    std::optional<uint32_t> result) override {
    hit_test_results_[hit_test_request_id] = result;
  }

  void SetRootID(uint32_t root_node_id) override {
    root_node_id_ = root_node_id;
  }

  inspect::Node GetInspectNode() override { return inspect::Node(); }

  float GetDeviceScaleFactor() override { return device_scale_factor_; }

  void SetDeviceScaleFactor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }

  const std::vector<fuchsia_accessibility_semantics::Node>& node_updates() {
    return node_updates_;
  }
  const std::vector<uint32_t>& node_deletions() { return node_deletions_; }
  const std::map<int, std::optional<uint32_t>>& hit_test_results() {
    return hit_test_results_;
  }

  const std::optional<uint32_t>& old_focus() { return old_focus_; }

  const std::optional<uint32_t>& new_focus() { return new_focus_; }

  const std::optional<uint32_t>& root_node_id() { return root_node_id_; }

  void reset() {
    node_updates_.clear();
    node_deletions_.clear();
    hit_test_results_.clear();
  }

 private:
  float device_scale_factor_ = 1.f;
  std::vector<fuchsia_accessibility_semantics::Node> node_updates_;
  std::vector<uint32_t> node_deletions_;
  std::map<int /* hit test request id */,
           std::optional<uint32_t> /* hit test result */>
      hit_test_results_;
  std::optional<uint32_t> old_focus_;
  std::optional<uint32_t> new_focus_;
  std::optional<uint32_t> root_node_id_;
};

class BrowserAccessibilityManagerFuchsiaTest : public testing::Test {
 public:
  BrowserAccessibilityManagerFuchsiaTest() = default;
  ~BrowserAccessibilityManagerFuchsiaTest() override = default;

 protected:
  const base::test::SingleThreadTaskEnvironment task_environment_;
  TestAXNodeIdDelegate node_id_delegate_;
  MockBrowserAccessibilityDelegate mock_browser_accessibility_delegate_;
  std::unique_ptr<BrowserAccessibilityManager> manager_{
      BrowserAccessibilityManager::Create(
          node_id_delegate_,
          &mock_browser_accessibility_delegate_)};
};

TEST_F(BrowserAccessibilityManagerFuchsiaTest, TestEmitNodeUpdates) {
  MockAccessibilityBridge mock_accessibility_bridge;
  AXTreeUpdate initial_state;
  AXTreeID tree_id = AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.tree_id = tree_id;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  manager_->ax_tree()->Unserialize(initial_state);

  {
    const auto& node_updates = mock_accessibility_bridge.node_updates();
    ASSERT_EQ(node_updates.size(), 1u);

    BrowserAccessibilityFuchsia* node_1 =
        ToBrowserAccessibilityFuchsia(manager_->GetFromID(1));
    ASSERT_TRUE(node_1);

    EXPECT_EQ(node_updates[0].node_id(), node_1->GetFuchsiaNodeID());

    // Verify that the the accessibility bridge root ID was set to node 1's
    // unique ID.
    ASSERT_TRUE(mock_accessibility_bridge.root_node_id().has_value());
    EXPECT_EQ(*mock_accessibility_bridge.root_node_id(),
              static_cast<uint32_t>(node_1->GetFuchsiaNodeID()));

    const auto& node_deletions = mock_accessibility_bridge.node_deletions();
    // The initial empty document root is the only node that was deleted.
    EXPECT_EQ(node_deletions.size(), 1u);
  }

  // Send another update for node 1, and verify that it was passed to the
  // accessibility bridge.
  AXTreeUpdate updated_state;
  updated_state.root_id = 1;
  updated_state.nodes.resize(2);
  updated_state.nodes[0].id = 1;
  updated_state.nodes[0].child_ids.push_back(2);
  updated_state.nodes[1].id = 2;

  manager_->ax_tree()->Unserialize(updated_state);

  {
    const auto& node_updates = mock_accessibility_bridge.node_updates();
    ASSERT_EQ(node_updates.size(), 3u);

    BrowserAccessibilityFuchsia* node_1 =
        ToBrowserAccessibilityFuchsia(manager_->GetFromID(1));
    ASSERT_TRUE(node_1);
    BrowserAccessibilityFuchsia* node_2 =
        ToBrowserAccessibilityFuchsia(manager_->GetFromID(2));
    ASSERT_TRUE(node_2);

    // Node 1 is the root of the root tree, so its fuchsia ID should be 0.
    EXPECT_EQ(node_updates[1].node_id(), node_1->GetFuchsiaNodeID());
    ASSERT_EQ(node_updates[1].child_ids()->size(), 1u);
    EXPECT_EQ(node_updates[1].child_ids().value()[0],
              node_2->GetFuchsiaNodeID());

    // Node 2 is NOT the root, so its fuchsia ID should be its AXUniqueID.
    EXPECT_EQ(node_updates[2].node_id().value(), node_2->GetFuchsiaNodeID());
  }
}

TEST_F(BrowserAccessibilityManagerFuchsiaTest, TestDeleteNodes) {
  MockAccessibilityBridge mock_accessibility_bridge;
  AXTreeUpdate initial_state;
  AXTreeID tree_id = AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.tree_id = tree_id;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;

  manager_->ax_tree()->Unserialize(initial_state);

  // Verify that no deletions were received.
  {
    const auto& node_deletions = mock_accessibility_bridge.node_deletions();
    // Only the initial empty document root has been deleted.
    EXPECT_EQ(node_deletions.size(), 1u);
  }

  // Get the fuchsia IDs for nodes 1 and 2 before they are deleted.
  BrowserAccessibilityFuchsia* node_1 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(1));
  ASSERT_TRUE(node_1);
  uint32_t node_1_fuchsia_id = node_1->GetFuchsiaNodeID();
  BrowserAccessibilityFuchsia* node_2 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(2));
  ASSERT_TRUE(node_2);
  uint32_t node_2_fuchsia_id = node_2->GetFuchsiaNodeID();

  // Delete node 2.
  AXTreeUpdate updated_state;
  updated_state.nodes.resize(1);
  updated_state.nodes[0].id = 1;

  manager_->ax_tree()->Unserialize(updated_state);

  // Verify that the accessibility bridge received a deletion for node 2.
  {
    const auto& node_deletions = mock_accessibility_bridge.node_deletions();
    // The initial empty document root has also been deleted, ignore that.
    ASSERT_EQ(node_deletions.size(), 2u);
    EXPECT_EQ(node_deletions[1], static_cast<uint32_t>(node_2_fuchsia_id));
  }

  // Destroy manager. Doing so should force the remainder of the tree to be
  // deleted.
  manager_.reset();

  // Verify that the accessibility bridge received a deletion for node 1.
  {
    const auto& node_deletions = mock_accessibility_bridge.node_deletions();
    // The initial empty document root has also been deleted, ignore that as
    // well as the previous node that had been deleted.
    ASSERT_EQ(node_deletions.size(), 3u);
    EXPECT_EQ(node_deletions[2], node_1_fuchsia_id);
  }
}

TEST_F(BrowserAccessibilityManagerFuchsiaTest, TestLocationChange) {
  MockAccessibilityBridge mock_accessibility_bridge;
  AXTreeUpdate initial_state;
  AXTreeID tree_id = AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.tree_id = tree_id;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;

  manager_->ax_tree()->Unserialize(initial_state);

  {
    const std::vector<fuchsia_accessibility_semantics::Node>& node_updates =
        mock_accessibility_bridge.node_updates();
    ASSERT_EQ(node_updates.size(), 2u);
  }

  // Send location update for node 2.
  AXLocationAndScrollUpdates changes;
  AXRelativeBounds relative_bounds;
  relative_bounds.bounds =
      gfx::RectF(/*x=*/1, /*y=*/2, /*width=*/3, /*height=*/4);
  changes.location_changes.emplace_back(2, relative_bounds);
  manager_->OnLocationChanges(std::move(changes));

  {
    BrowserAccessibilityFuchsia* node_2 =
        ToBrowserAccessibilityFuchsia(manager_->GetFromID(2));
    ASSERT_TRUE(node_2);

    const std::vector<fuchsia_accessibility_semantics::Node>& node_updates =
        mock_accessibility_bridge.node_updates();
    ASSERT_EQ(node_updates.size(), 3u);
    const fuchsia_accessibility_semantics::Node& node_update =
        node_updates.back();
    EXPECT_EQ(node_update.node_id(),
              static_cast<uint32_t>(node_2->GetFuchsiaNodeID()));
    ASSERT_TRUE(node_update.location());
    const fuchsia_ui_gfx::BoundingBox& location =
        node_update.location().value();
    EXPECT_EQ(location.min().x(), 1);
    EXPECT_EQ(location.min().y(), 2);
    EXPECT_EQ(location.max().x(), 4);
    EXPECT_EQ(location.max().y(), 6);
  }
}

TEST_F(BrowserAccessibilityManagerFuchsiaTest, TestFocusChange) {
  MockAccessibilityBridge mock_accessibility_bridge;
  // We need to specify that this is the root frame; otherwise, no focus events
  // will be fired. Likewise, we need to ensure that events are not suppressed.
  mock_browser_accessibility_delegate_.is_root_frame_ = true;
  BrowserAccessibilityManager::NeverSuppressOrDelayEventsForTesting();

  AXTreeUpdate initial_state;
  AXTreeID tree_id = AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.tree_id = tree_id;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  initial_state.tree_data.parent_tree_id = AXTreeIDUnknown();
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;

  manager_->ax_tree()->Unserialize(initial_state);

  BrowserAccessibilityFuchsia* node_1 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(1));
  ASSERT_TRUE(node_1);

  BrowserAccessibilityFuchsia* node_2 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(2));
  ASSERT_TRUE(node_2);

  // Set focus to node 1, and check that the focus was updated from null to
  // node 1.
  {
    AXUpdatesAndEvents event;
    AXTreeUpdate updated_state;
    updated_state.tree_data.tree_id = tree_id;
    updated_state.has_tree_data = true;
    updated_state.tree_data.focused_tree_id = tree_id;
    updated_state.tree_data.focus_id = 1;
    event.ax_tree_id = tree_id;
    event.updates.push_back(std::move(updated_state));
    EXPECT_TRUE(manager_->OnAccessibilityEvents(event));
  }

  {
    const std::vector<fuchsia_accessibility_semantics::Node>& node_updates =
        mock_accessibility_bridge.node_updates();
    ASSERT_FALSE(node_updates.empty());
    EXPECT_EQ(node_updates.back().node_id().value(),
              node_1->GetFuchsiaNodeID());
    ASSERT_TRUE(node_updates.back().states());
    ASSERT_TRUE(node_updates.back().states()->has_input_focus().has_value());
    EXPECT_TRUE(node_updates.back().states()->has_input_focus().value());
  }

  // Set focus to node 2, and check that focus was updated from node 1 to node
  // 2.
  {
    AXUpdatesAndEvents event;
    AXTreeUpdate updated_state;
    updated_state.tree_data.tree_id = tree_id;
    updated_state.has_tree_data = true;
    updated_state.tree_data.focused_tree_id = tree_id;
    updated_state.tree_data.focus_id = 2;
    event.ax_tree_id = tree_id;
    event.updates.push_back(std::move(updated_state));
    EXPECT_TRUE(manager_->OnAccessibilityEvents(event));
  }

  {
    const std::vector<fuchsia_accessibility_semantics::Node>& node_updates =
        mock_accessibility_bridge.node_updates();
    ASSERT_GT(node_updates.size(), 2u);
    const fuchsia_accessibility_semantics::Node& old_focus_node =
        node_updates[node_updates.size() - 2];
    EXPECT_EQ(old_focus_node.node_id().value(), node_1->GetFuchsiaNodeID());
    ASSERT_TRUE(old_focus_node.states());
    ASSERT_TRUE(old_focus_node.states()->has_input_focus().has_value());
    EXPECT_FALSE(old_focus_node.states()->has_input_focus().value());
    EXPECT_EQ(node_updates.back().node_id().value(),
              node_2->GetFuchsiaNodeID());
    ASSERT_TRUE(node_updates.back().states());
    ASSERT_TRUE(node_updates.back().states()->has_input_focus().has_value());
    EXPECT_TRUE(node_updates.back().states()->has_input_focus().value());
  }
}

TEST_F(BrowserAccessibilityManagerFuchsiaTest, HitTest) {
  MockAccessibilityBridge mock_accessibility_bridge;
  mock_browser_accessibility_delegate_.is_root_frame_ = true;

  AXTreeUpdate initial_state;
  AXTreeID tree_id = AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.tree_id = tree_id;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;

  manager_->ax_tree()->Unserialize(initial_state);

  BrowserAccessibilityFuchsia* node_1 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(1));
  ASSERT_TRUE(node_1);
  BrowserAccessibilityFuchsia* node_2 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(2));
  ASSERT_TRUE(node_2);

  // Set the hit test action data. Note that we will later hard-code the result
  // of the hit test, so the geometry doesn't matter. We just need to verify
  // that the target point specified here matches the target point received by
  // the delegate.
  AXActionData action_data;
  action_data.action = ax::mojom::Action::kHitTest;
  action_data.target_point.set_x(1);
  action_data.target_point.set_y(2);
  action_data.request_id = 3;

  AXPlatformNodeFuchsia* platform_node = static_cast<AXPlatformNodeFuchsia*>(
      AXPlatformNodeBase::GetFromUniqueId(node_1->GetFuchsiaNodeID()));
  ASSERT_TRUE(platform_node);

  platform_node->PerformAction(action_data);

  {
    std::optional<gfx::Point> last_target =
        mock_browser_accessibility_delegate_.last_hit_test_point();
    ASSERT_TRUE(last_target.has_value());
    EXPECT_EQ(last_target->x(), 1);
    EXPECT_EQ(last_target->y(), 2);

    std::optional<int> last_request_id =
        mock_browser_accessibility_delegate_.last_request_id();
    ASSERT_TRUE(last_request_id.has_value());
    EXPECT_EQ(*last_request_id, action_data.request_id);
  }

  // Fire blink event to signify the hit test result.
  manager_->FireBlinkEvent(ax::mojom::Event::kHover, node_2,
                           action_data.request_id);

  {
    const std::map<int, std::optional<uint32_t>>& hit_test_results =
        mock_accessibility_bridge.hit_test_results();
    // We should have a hit test result for request id = 3, and the result
    // should be the fuchsia ID of node 2, which is our hit result specified
    // above.
    ASSERT_TRUE(hit_test_results.count(3));
    ASSERT_TRUE(hit_test_results.at(3).has_value());
    EXPECT_EQ(*hit_test_results.at(3), node_2->GetFuchsiaNodeID());
  }
}

TEST_F(BrowserAccessibilityManagerFuchsiaTest, HitTestFails) {
  MockAccessibilityBridge mock_accessibility_bridge;
  mock_browser_accessibility_delegate_.is_root_frame_ = true;

  AXTreeUpdate initial_state;
  AXTreeID tree_id = AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.tree_id = tree_id;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;

  manager_->ax_tree()->Unserialize(initial_state);

  BrowserAccessibilityFuchsia* node_1 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(1));
  ASSERT_TRUE(node_1);

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kHitTest;
  action_data.target_point.set_x(1);
  action_data.target_point.set_y(2);
  action_data.request_id = 4;

  AXPlatformNodeFuchsia* platform_node = static_cast<AXPlatformNodeFuchsia*>(
      AXPlatformNodeBase::GetFromUniqueId(node_1->GetFuchsiaNodeID()));
  ASSERT_TRUE(platform_node);

  platform_node->PerformAction(action_data);

  {
    std::optional<gfx::Point> last_target =
        mock_browser_accessibility_delegate_.last_hit_test_point();
    EXPECT_EQ(last_target->x(), 1);
    EXPECT_EQ(last_target->y(), 2);
  }

  // FIre blink event to signify the hit test result.
  manager_->FireBlinkEvent(ax::mojom::Event::kHover, nullptr, 4);

  {
    const std::map<int, std::optional<uint32_t>>& hit_test_results =
        mock_accessibility_bridge.hit_test_results();

    ASSERT_FALSE(hit_test_results.empty());
    ASSERT_TRUE(hit_test_results.count(4));
    EXPECT_FALSE(hit_test_results.at(4).has_value());
  }
}

TEST_F(BrowserAccessibilityManagerFuchsiaTest, PerformAction) {
  mock_browser_accessibility_delegate_.is_root_frame_ = true;

  AXTreeUpdate initial_state;
  AXTreeID tree_id = AXTreeID::CreateNewAXTreeID();
  initial_state.tree_data.tree_id = tree_id;
  initial_state.has_tree_data = true;
  initial_state.tree_data.loaded = true;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;

  manager_->ax_tree()->Unserialize(initial_state);

  BrowserAccessibilityFuchsia* node_2 =
      ToBrowserAccessibilityFuchsia(manager_->GetFromID(2));
  ASSERT_TRUE(node_2);

  AXActionData action_data;
  action_data.action = ax::mojom::Action::kScrollToMakeVisible;
  action_data.target_node_id = 2;

  AXPlatformNodeFuchsia* platform_node = static_cast<AXPlatformNodeFuchsia*>(
      AXPlatformNodeBase::GetFromUniqueId(node_2->GetFuchsiaNodeID()));
  ASSERT_TRUE(platform_node);

  platform_node->PerformAction(action_data);

  {
    const std::optional<AXActionData> last_action_data =
        mock_browser_accessibility_delegate_.last_action_data();
    ASSERT_TRUE(last_action_data);
    EXPECT_EQ(last_action_data->action,
              ax::mojom::Action::kScrollToMakeVisible);
  }
}

}  // namespace
}  // namespace ui
