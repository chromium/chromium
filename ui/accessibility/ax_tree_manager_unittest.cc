// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_tree_manager.h"

#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/test_single_ax_tree_manager.h"

namespace ui {

namespace {

struct FiredGeneratedEvent {
  AXEventGenerator::Event event_type;
  AXNodeID node_id;
};

class TestMenuPopupAXTreeManager : public TestSingleAXTreeManager {
 public:
  explicit TestMenuPopupAXTreeManager(std::unique_ptr<AXTree> tree)
      : TestSingleAXTreeManager(std::move(tree)) {}

  void FireGeneratedEvent(AXEventGenerator::Event event_type,
                          const AXNode* node) override {
    fired_events_.push_back({event_type, node->id()});
  }

  const std::vector<FiredGeneratedEvent>& fired_events() const {
    return fired_events_;
  }

 private:
  std::vector<FiredGeneratedEvent> fired_events_;
};

AXTreeUpdate CreateRootOnlyUpdate() {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  AXTreeUpdate update;
  update.root_id = root.id;
  update.nodes = {root};
  return update;
}

AXTreeUpdate CreateMenuUpdate(bool ignored) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids = {2};

  AXNodeData menu;
  menu.id = 2;
  menu.role = ax::mojom::Role::kMenu;
  if (ignored) {
    menu.AddState(ax::mojom::State::kIgnored);
  }

  AXTreeUpdate update;
  update.root_id = root.id;
  update.nodes = {root, menu};
  return update;
}

AXTreeUpdate CreateInitialUpdate(AXTreeUpdate update) {
  update.has_tree_data = true;
  update.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  return update;
}

bool HasGeneratedEvent(const AXEventGenerator& event_generator,
                       AXEventGenerator::Event event_type,
                       AXNodeID node_id) {
  for (const AXEventGenerator::TargetedEvent& event : event_generator) {
    if (event.event_params->event == event_type && event.node_id == node_id) {
      return true;
    }
  }
  return false;
}

int CountFiredEvents(const TestMenuPopupAXTreeManager& manager,
                     AXEventGenerator::Event event_type,
                     AXNodeID node_id) {
  int count = 0;
  for (const FiredGeneratedEvent& event : manager.fired_events()) {
    if (event.event_type == event_type && event.node_id == node_id) {
      ++count;
    }
  }
  return count;
}

}  // namespace

TEST(AXTreeManagerTest, ConstructFromInitialState) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  std::string name = "Hello";
  root.AddStringAttribute(ax::mojom::StringAttribute::kName, name);

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.push_back(root);
  initial_state.has_tree_data = true;

  TestSingleAXTreeManager manager(std::make_unique<AXSerializableTree>());

  manager.Initialize(initial_state);

  AXNode* returned_root = manager.GetRoot();
  ASSERT_EQ(
      returned_root->GetStringAttribute(ax::mojom::StringAttribute::kName),
      name);
}

TEST(AXTreeManagerTest, GetRootManagerUnserializedParent) {
  // The test covers the scenario where a manager's parent tree is not
  // yet serialized.
  // ++1 kRootWebArea
  // ++++2 kGenericContainer
  // ++++3 kGenericContainer
  // ++++++4 kRootWebArea
  // ++++++++5 kGenericContainer
  // ++++++++++6 kRootWebArea
  // ++++++++++++7 kGenericContainer

  AXNodeData root_1;
  AXNodeData generic_container_2;
  AXNodeData generic_container_3;
  AXNodeData root_4;
  AXNodeData generic_container_5;
  AXNodeData root_6;
  AXNodeData generic_container_7;

  root_1.id = 1;
  generic_container_2.id = 2;
  generic_container_3.id = 3;
  root_4.id = 4;
  generic_container_5.id = 5;
  root_6.id = 6;
  generic_container_7.id = 7;

  root_1.role = ax::mojom::Role::kRootWebArea;
  root_1.child_ids = {generic_container_2.id, generic_container_3.id};

  generic_container_2.role = ax::mojom::Role::kGenericContainer;

  generic_container_3.role = ax::mojom::Role::kGenericContainer;

  root_4.role = ax::mojom::Role::kRootWebArea;
  root_4.child_ids = {generic_container_5.id};

  generic_container_5.role = ax::mojom::Role::kGenericContainer;

  root_6.role = ax::mojom::Role::kRootWebArea;
  root_6.child_ids = {generic_container_7.id};

  generic_container_7.role = ax::mojom::Role::kGenericContainer;

  AXTreeUpdate first_state;
  first_state.root_id = root_1.id;
  first_state.nodes = {root_1, generic_container_2, generic_container_3};
  first_state.has_tree_data = true;
  first_state.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();

  AXTreeUpdate middle_state;
  middle_state.root_id = root_4.id;
  middle_state.nodes = {root_4, generic_container_5};
  middle_state.has_tree_data = true;
  middle_state.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();

  AXTreeUpdate last_state;
  last_state.root_id = root_6.id;
  last_state.nodes = {root_6, generic_container_7};
  last_state.has_tree_data = true;
  last_state.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();

  // We link the first two child trees to their parent trees.
  middle_state.tree_data.parent_tree_id = first_state.tree_data.tree_id;
  generic_container_3.AddChildTreeId(middle_state.tree_data.tree_id);

  // We link the last child to the middle child.
  last_state.tree_data.parent_tree_id = middle_state.tree_data.tree_id;
  generic_container_5.AddChildTreeId(last_state.tree_data.tree_id);

  // Create the managers. We don't `Initialize` the middle manager to test the
  // scenario where the parent manager is not serialized yet.
  TestSingleAXTreeManager first_manager(std::make_unique<AXSerializableTree>());
  first_manager.Initialize(first_state);
  TestSingleAXTreeManager last_manager(std::make_unique<AXSerializableTree>());
  last_manager.Initialize(last_state);

  ASSERT_EQ(first_manager.GetRootManager(), &first_manager);

  ASSERT_EQ(last_manager.GetRootManager(), nullptr);

}

TEST(AXTreeManagerTest, GetRootManagerAndIsRoot) {
  // The test covers two cases: First, when we call GetRootManager on the root
  // manager itself. Second, when we call GetRootManager on a child of the root.
  // ++1 kRootWebArea
  // ++++2 kGenericContainer
  // ++++3 kGenericContainer
  // ++++++4 kRootWebArea
  // ++++++++5 kGenericContainer
  // ++++++++++6 kRootWebArea
  // ++++++++++++7 kGenericContainer

  AXNodeData root_1;
  AXNodeData generic_container_2;
  AXNodeData generic_container_3;
  AXNodeData root_4;
  AXNodeData generic_container_5;
  AXNodeData root_6;
  AXNodeData generic_container_7;

  root_1.id = 1;
  generic_container_2.id = 2;
  generic_container_3.id = 3;
  root_4.id = 4;
  generic_container_5.id = 5;
  root_6.id = 6;
  generic_container_7.id = 7;

  root_1.role = ax::mojom::Role::kRootWebArea;
  root_1.child_ids = {generic_container_2.id, generic_container_3.id};

  generic_container_2.role = ax::mojom::Role::kGenericContainer;

  generic_container_3.role = ax::mojom::Role::kGenericContainer;

  root_4.role = ax::mojom::Role::kRootWebArea;
  root_4.child_ids = {generic_container_5.id};

  generic_container_5.role = ax::mojom::Role::kGenericContainer;

  root_6.role = ax::mojom::Role::kRootWebArea;
  root_6.child_ids = {generic_container_7.id};

  generic_container_7.role = ax::mojom::Role::kGenericContainer;

  AXTreeUpdate first_state;
  first_state.root_id = root_1.id;
  first_state.nodes = {root_1, generic_container_2, generic_container_3};
  first_state.has_tree_data = true;
  first_state.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();

  AXTreeUpdate middle_state;
  middle_state.root_id = root_4.id;
  middle_state.nodes = {root_4, generic_container_5};
  middle_state.has_tree_data = true;
  middle_state.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();

  AXTreeUpdate last_state;
  last_state.root_id = root_6.id;
  last_state.nodes = {root_6, generic_container_7};
  last_state.has_tree_data = true;
  last_state.tree_data.tree_id = AXTreeID::CreateNewAXTreeID();

  // We link the first two child trees to their parent trees.
  middle_state.tree_data.parent_tree_id = first_state.tree_data.tree_id;
  generic_container_3.AddChildTreeId(middle_state.tree_data.tree_id);

  // We link the last child to the middle child.
  last_state.tree_data.parent_tree_id = middle_state.tree_data.tree_id;
  generic_container_5.AddChildTreeId(last_state.tree_data.tree_id);

  // Create the managers.
  TestSingleAXTreeManager first_manager(std::make_unique<AXSerializableTree>());
  first_manager.Initialize(first_state);
  TestSingleAXTreeManager middle_manager(
      std::make_unique<AXSerializableTree>());
  middle_manager.Initialize(middle_state);
  TestSingleAXTreeManager last_manager(std::make_unique<AXSerializableTree>());
  last_manager.Initialize(last_state);

  ASSERT_EQ(first_manager.GetRootManager(), &first_manager);
  ASSERT_EQ(middle_manager.GetRootManager(), &first_manager);
  ASSERT_EQ(last_manager.GetRootManager(), &first_manager);
}

TEST(AXTreeManagerTest, MenuPopupDeleteWhileExposedFiresEnd) {
  TestMenuPopupAXTreeManager manager(
      std::make_unique<AXTree>(CreateInitialUpdate(CreateRootOnlyUpdate())));

  ASSERT_TRUE(manager.GetTree()->Unserialize(CreateMenuUpdate(false)));
  EXPECT_TRUE(HasGeneratedEvent(manager.event_generator(),
                                AXEventGenerator::Event::MENU_POPUP_START, 2));

  manager.event_generator().ClearEvents();
  ASSERT_TRUE(manager.GetTree()->Unserialize(CreateRootOnlyUpdate()));

  ASSERT_EQ(1u, manager.fired_events().size());
  EXPECT_EQ(AXEventGenerator::Event::MENU_POPUP_END,
            manager.fired_events()[0].event_type);
  EXPECT_EQ(2, manager.fired_events()[0].node_id);
}

TEST(AXTreeManagerTest, MenuPopupDeleteWhileIgnoredDoesNotFireEnd) {
  TestMenuPopupAXTreeManager manager(
      std::make_unique<AXTree>(CreateInitialUpdate(CreateRootOnlyUpdate())));

  ASSERT_TRUE(manager.GetTree()->Unserialize(CreateMenuUpdate(true)));
  manager.event_generator().ClearEvents();

  ASSERT_TRUE(manager.GetTree()->Unserialize(CreateRootOnlyUpdate()));

  EXPECT_TRUE(manager.fired_events().empty());
}

TEST(AXTreeManagerTest, MenuPopupHideThenDeleteFiresOneEnd) {
  TestMenuPopupAXTreeManager manager(
      std::make_unique<AXTree>(CreateInitialUpdate(CreateRootOnlyUpdate())));

  ASSERT_TRUE(manager.GetTree()->Unserialize(CreateMenuUpdate(false)));
  ASSERT_TRUE(HasGeneratedEvent(manager.event_generator(),
                                AXEventGenerator::Event::MENU_POPUP_START, 2));
  manager.event_generator().ClearEvents();

  ASSERT_TRUE(manager.GetTree()->Unserialize(CreateMenuUpdate(true)));
  ASSERT_TRUE(HasGeneratedEvent(manager.event_generator(),
                                AXEventGenerator::Event::MENU_POPUP_END, 2));
  manager.FireGeneratedEvent(AXEventGenerator::Event::MENU_POPUP_END,
                             manager.GetNode(2));
  manager.event_generator().ClearEvents();

  ASSERT_TRUE(manager.GetTree()->Unserialize(CreateRootOnlyUpdate()));

  EXPECT_EQ(
      1, CountFiredEvents(manager, AXEventGenerator::Event::MENU_POPUP_END, 2));
}

}  // namespace ui
