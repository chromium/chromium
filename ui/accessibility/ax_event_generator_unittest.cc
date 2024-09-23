// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_event_generator.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace ui {

// Required by gmock to print TargetedEvent in a human-readable way.
void PrintTo(const AXEventGenerator::TargetedEvent& event, std::ostream* os) {
  *os << event.event_params->event << " on " << event.node_id;
}

namespace {

using testing::IsEmpty;
using testing::IsSupersetOf;
using testing::Matches;
using testing::PrintToString;
using testing::UnorderedElementsAre;

MATCHER_P2(HasEventAtNode,
           expected_event_type,
           expected_node_id,
           std::string(negation ? "does not have" : "has") + " " +
               PrintToString(expected_event_type) + " on " +
               PrintToString(expected_node_id)) {
  const auto& event = arg;
  std::string failure_message;
  if (!Matches(expected_event_type)(event.event_params->event)) {
    failure_message +=
        "Expected event type: " + PrintToString(expected_event_type) +
        ", actual event type: " + PrintToString(event.event_params->event);
  }
  if (!Matches(expected_node_id)(event.node_id)) {
    if (!failure_message.empty()) {
      failure_message += "; ";
    }
    failure_message += "Expected node id: " + PrintToString(expected_node_id) +
                       ", actual node id: " + PrintToString(event.node_id);
  }
  if (!failure_message.empty()) {
    *result_listener << failure_message;
    return false;
  }
  return true;
}

}  // namespace

TEST(AXEventGeneratorTest, IterateThroughEmptyEventSets) {
  // The event map contains the following:
  // node1, <>
  // node2, <>
  // node3, <IGNORED_CHANGED, SUBTREE_CREATED, NAME_CHANGED>
  // node4, <>
  // node5, <>
  // node6, <>
  // node7, <IGNORED_CHANGED>
  // node8, <>
  // node9, <>
  // Verify AXEventGenerator can iterate through empty event sets, and returning
  // the correct events.
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(9);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);

  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids.push_back(3);

  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].child_ids.push_back(4);

  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].child_ids.push_back(5);

  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].child_ids.push_back(6);

  initial_state.nodes[5].id = 6;
  initial_state.nodes[5].child_ids.push_back(7);

  initial_state.nodes[6].id = 7;
  initial_state.nodes[6].child_ids.push_back(8);

  initial_state.nodes[7].id = 8;
  initial_state.nodes[7].child_ids.push_back(9);

  initial_state.nodes[8].id = 9;
  initial_state.has_tree_data = true;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  AXNode* node1 = tree.root();
  AXNode* node2 = tree.GetFromId(2);
  AXNode* node3 = tree.GetFromId(3);
  AXNode* node4 = tree.GetFromId(4);
  AXNode* node5 = tree.GetFromId(5);
  AXNode* node6 = tree.GetFromId(6);
  AXNode* node7 = tree.GetFromId(7);
  AXNode* node8 = tree.GetFromId(8);
  AXNode* node9 = tree.GetFromId(9);

  // Node1 contains no event.
  std::set<AXEventGenerator::EventParams> node1_events;
  // Node2 contains no event.
  std::set<AXEventGenerator::EventParams> node2_events;
  // Node3 contains IGNORED_CHANGED, SUBTREE_CREATED, NAME_CHANGED.
  std::set<AXEventGenerator::EventParams> node3_events;
  node3_events.emplace(AXEventGenerator::Event::IGNORED_CHANGED,
                       ax::mojom::EventFrom::kNone, ax::mojom::Action::kNone,
                       std::vector<AXEventIntent>());
  node3_events.emplace(AXEventGenerator::Event::SUBTREE_CREATED,
                       ax::mojom::EventFrom::kNone, ax::mojom::Action::kNone,
                       std::vector<AXEventIntent>());
  node3_events.emplace(AXEventGenerator::Event::NAME_CHANGED,
                       ax::mojom::EventFrom::kNone, ax::mojom::Action::kNone,
                       std::vector<AXEventIntent>());
  // Node4 contains no event.
  std::set<AXEventGenerator::EventParams> node4_events;
  // Node5 contains no event.
  std::set<AXEventGenerator::EventParams> node5_events;
  // Node6 contains no event.
  std::set<AXEventGenerator::EventParams> node6_events;
  // Node7 contains IGNORED_CHANGED.
  std::set<AXEventGenerator::EventParams> node7_events;
  node7_events.emplace(AXEventGenerator::Event::IGNORED_CHANGED,
                       ax::mojom::EventFrom::kNone, ax::mojom::Action::kNone,
                       std::vector<AXEventIntent>());
  // Node8 contains no event.
  std::set<AXEventGenerator::EventParams> node8_events;
  // Node9 contains no event.
  std::set<AXEventGenerator::EventParams> node9_events;

  event_generator.AddEventsForTesting(*node1, node1_events);
  event_generator.AddEventsForTesting(*node2, node2_events);
  event_generator.AddEventsForTesting(*node3, node3_events);
  event_generator.AddEventsForTesting(*node4, node4_events);
  event_generator.AddEventsForTesting(*node5, node5_events);
  event_generator.AddEventsForTesting(*node6, node6_events);
  event_generator.AddEventsForTesting(*node7, node7_events);
  event_generator.AddEventsForTesting(*node8, node8_events);
  event_generator.AddEventsForTesting(*node9, node9_events);

  std::map<AXNode*, std::set<AXEventGenerator::Event>> expected_event_map;
  expected_event_map[node3] = {AXEventGenerator::Event::IGNORED_CHANGED,
                               AXEventGenerator::Event::SUBTREE_CREATED,
                               AXEventGenerator::Event::NAME_CHANGED};
  expected_event_map[node7] = {AXEventGenerator::Event::IGNORED_CHANGED};

  for (const auto& targeted_event : event_generator) {
    AXNode* node = tree.GetFromId(targeted_event.node_id);
    ASSERT_NE(nullptr, node);
    auto map_iter = expected_event_map.find(node);

    ASSERT_NE(map_iter, expected_event_map.end())
        << "|expected_event_map| contains node_id=" << targeted_event.node_id
        << "\nExpected: true"
        << "\nActual: " << std::boolalpha
        << (map_iter != expected_event_map.end());

    std::set<AXEventGenerator::Event>& node_events = map_iter->second;
    auto event_iter = node_events.find(targeted_event.event_params->event);

    ASSERT_NE(event_iter, node_events.end())
        << "Event=" << targeted_event.event_params->event
        << ", on node_id=" << targeted_event.node_id
        << " NOT found in |expected_event_map|";

    // If the event from |event_generator| is found in |expected_event_map|,
    // we want to delete the corresponding entry in |expected_event_map|.
    node_events.erase(event_iter);
    if (node_events.empty())
      expected_event_map.erase(map_iter);
  }

  // We should expect |expected_event_map_| to be empty, when all the generated
  // events match expected events.
  EXPECT_TRUE(expected_event_map.empty());
}

TEST(AXEventGeneratorTest, DocumentSelectionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.has_tree_data = true;
  initial_state.tree_data.sel_focus_object_id = 1;
  initial_state.tree_data.sel_focus_offset = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.tree_data.sel_focus_offset = 2;

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, DocumentTitleChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.has_tree_data = true;
  initial_state.tree_data.title = "Before";
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.tree_data.title = "After";

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::DOCUMENT_TITLE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, ExpandedAndRowCount) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kTable;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kRow;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kComboBoxSelect;
  initial_state.nodes[3].AddState(ax::mojom::State::kExpanded);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[2].AddState(ax::mojom::State::kExpanded);
  update.nodes[3].RemoveState(ax::mojom::State::kExpanded);
  update.nodes[3].AddState(ax::mojom::State::kCollapsed);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::COLLAPSED, 4),
          HasEventAtNode(AXEventGenerator::Event::EXPANDED, 3),
          HasEventAtNode(AXEventGenerator::Event::ROW_COUNT_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::STATE_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         3),
          HasEventAtNode(AXEventGenerator::Event::STATE_CHANGED, 4),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         4)));
}

TEST(AXEventGeneratorTest, SelectedAndSelectedChildren) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kMenu;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kMenuItem;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kListBoxOption;
  initial_state.nodes[3].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                          true);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[2].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  update.nodes.pop_back();
  update.nodes.emplace_back();
  update.nodes[3].id = 4;
  update.nodes[3].role = ax::mojom::Role::kListBoxOption;
  update.nodes[3].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::SELECTED_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         3),
          HasEventAtNode(AXEventGenerator::Event::SELECTED_CHANGED, 4),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         4)));
}

TEST(AXEventGeneratorTest, SelectedAndSelectedValueChanged) {
  // This test is based on the following HTML snippet which produces the below
  // simplified accessibility tree.
  //
  // <select>
  //   <option selected>Item 1</option>
  //   <option>Item 2</option>
  // </select>
  // <select size="2">
  //   <option>Item 1</option>
  //   <option selected>Item 2</option>
  // </select>
  //
  // kRootWebArea
  // ++kComboBoxSelect value="Item 1"
  // ++++kMenuListPopup invisible
  // ++++++kMenuListOption name="Item 1" selected=true
  // ++++++kMenuListOption name="Item 2" selected=false
  // ++kListBox value="Item 2"
  // ++++kListBoxOption name="Item 1" selected=false
  // ++++kListBoxOption name="Item 2" selected=true

  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  AXNodeData popup_button;
  popup_button.id = 2;
  popup_button.role = ax::mojom::Role::kComboBoxSelect;
  popup_button.SetValue("Item 1");

  AXNodeData menu_list_popup;
  menu_list_popup.id = 3;
  menu_list_popup.role = ax::mojom::Role::kMenuListPopup;
  menu_list_popup.AddState(ax::mojom::State::kInvisible);

  AXNodeData menu_list_option_1;
  menu_list_option_1.id = 4;
  menu_list_option_1.role = ax::mojom::Role::kMenuListOption;
  menu_list_option_1.SetName("Item 1");
  menu_list_option_1.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                      true);

  AXNodeData menu_list_option_2;
  menu_list_option_2.id = 5;
  menu_list_option_2.role = ax::mojom::Role::kMenuListOption;
  menu_list_option_2.SetName("Item 2");

  AXNodeData list_box;
  list_box.id = 6;
  list_box.role = ax::mojom::Role::kListBox;
  list_box.SetValue("Item 2");

  AXNodeData list_box_option_1;
  list_box_option_1.id = 7;
  list_box_option_1.role = ax::mojom::Role::kListBoxOption;
  list_box_option_1.SetName("Item 1");

  AXNodeData list_box_option_2;
  list_box_option_2.id = 8;
  list_box_option_2.role = ax::mojom::Role::kRootWebArea;
  list_box_option_2.SetName("Item 2");
  list_box_option_2.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);

  popup_button.child_ids = {menu_list_popup.id};
  menu_list_popup.child_ids = {menu_list_option_1.id, menu_list_option_2.id};
  list_box.child_ids = {list_box_option_1.id, list_box_option_2.id};
  root.child_ids = {popup_button.id, list_box.id};

  AXTreeUpdate initial_state;
  initial_state.root_id = root.id;
  initial_state.nodes = {root,
                         popup_button,
                         menu_list_popup,
                         menu_list_option_1,
                         menu_list_option_2,
                         list_box,
                         list_box_option_1,
                         list_box_option_2};

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());

  popup_button.SetValue("Item 2");
  menu_list_option_1.RemoveBoolAttribute(ax::mojom::BoolAttribute::kSelected);
  menu_list_option_2.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                      true);
  list_box.SetValue("Item 1");
  list_box_option_1.AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  list_box_option_2.RemoveBoolAttribute(ax::mojom::BoolAttribute::kSelected);

  AXTreeUpdate update;
  update.nodes = {popup_button, menu_list_option_1, menu_list_option_2,
                  list_box,     list_box_option_1,  list_box_option_2};

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      IsSupersetOf(
          {HasEventAtNode(AXEventGenerator::Event::SELECTED_VALUE_CHANGED,
                          popup_button.id),
           HasEventAtNode(AXEventGenerator::Event::SELECTED_CHANGED,
                          menu_list_option_1.id),
           HasEventAtNode(AXEventGenerator::Event::SELECTED_CHANGED,
                          menu_list_option_2.id),
           HasEventAtNode(AXEventGenerator::Event::SELECTED_VALUE_CHANGED,
                          list_box.id),
           HasEventAtNode(AXEventGenerator::Event::SELECTED_CHANGED,
                          list_box_option_1.id),
           HasEventAtNode(AXEventGenerator::Event::SELECTED_CHANGED,
                          list_box_option_2.id)}));
}

TEST(AXEventGeneratorTest, SelectionInTextFieldChanged) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  AXNodeData text_field;
  text_field.id = 2;
  text_field.role = ax::mojom::Role::kTextField;
  text_field.SetValue("Testing");
  text_field.AddState(ax::mojom::State::kEditable);

  root.child_ids = {text_field.id};

  AXTreeUpdate initial_state;
  initial_state.root_id = root.id;
  initial_state.nodes = {root, text_field};

  AXTreeData tree_data;
  tree_data.sel_anchor_object_id = text_field.id;
  tree_data.sel_anchor_offset = 0;
  tree_data.sel_focus_object_id = text_field.id;
  tree_data.sel_focus_offset = 0;
  initial_state.tree_data = tree_data;
  initial_state.has_tree_data = true;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());

  {
    tree_data.sel_anchor_object_id = text_field.id;
    tree_data.sel_anchor_offset = 0;
    tree_data.sel_focus_object_id = text_field.id;
    tree_data.sel_focus_offset = 2;
    AXTreeUpdate update;
    update.tree_data = tree_data;
    update.has_tree_data = true;

    ASSERT_TRUE(tree.Unserialize(update));
    EXPECT_THAT(
        event_generator,
        UnorderedElementsAre(
            HasEventAtNode(AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED,
                           root.id),
            HasEventAtNode(AXEventGenerator::Event::TEXT_SELECTION_CHANGED,
                           text_field.id)));
  }

  event_generator.ClearEvents();
  {
    // A selection that does not include a text field in it should not raise the
    // "TEXT_SELECTION_CHANGED" event.
    tree_data.sel_anchor_object_id = root.id;
    tree_data.sel_anchor_offset = 0;
    tree_data.sel_focus_object_id = root.id;
    tree_data.sel_focus_offset = 0;
    AXTreeUpdate update;
    update.tree_data = tree_data;
    update.has_tree_data = true;

    ASSERT_TRUE(tree.Unserialize(update));
    EXPECT_THAT(
        event_generator,
        UnorderedElementsAre(HasEventAtNode(
            AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED, root.id)));
  }

  event_generator.ClearEvents();
  {
    // A selection that spans more than one node but which nevertheless ends on
    // a text field should still raise the "TEXT_SELECTION_CHANGED"
    // event.
    tree_data.sel_anchor_object_id = root.id;
    tree_data.sel_anchor_offset = 0;
    tree_data.sel_focus_object_id = text_field.id;
    tree_data.sel_focus_offset = 2;
    AXTreeUpdate update;
    update.tree_data = tree_data;
    update.has_tree_data = true;

    ASSERT_TRUE(tree.Unserialize(update));
    EXPECT_THAT(
        event_generator,
        UnorderedElementsAre(
            HasEventAtNode(AXEventGenerator::Event::DOCUMENT_SELECTION_CHANGED,
                           root.id),
            HasEventAtNode(AXEventGenerator::Event::TEXT_SELECTION_CHANGED,
                           text_field.id)));
  }
}

TEST(AXEventGeneratorTest, ValueInTextFieldChanged) {
  AXNodeData text_field;
  text_field.id = 1;
  text_field.role = ax::mojom::Role::kTextField;
  text_field.AddState(ax::mojom::State::kEditable);
  text_field.SetValue("Before");

  AXTreeUpdate initial_state;
  initial_state.root_id = text_field.id;
  initial_state.nodes = {text_field};

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());

  text_field.SetValue("After");
  AXTreeUpdate update;
  update.nodes = {text_field};

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::VALUE_IN_TEXT_FIELD_CHANGED,
                  text_field.id)));
}

TEST(AXEventGeneratorTest, FloatValueChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kSlider;
  initial_state.nodes[0].AddFloatAttribute(
      ax::mojom::FloatAttribute::kValueForRange, 1.0);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[0].float_attributes.clear();
  update.nodes[0].AddFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                                    2.0);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::RANGE_VALUE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, InvalidStatusChanged) {
  AXNodeData text_field;
  text_field.id = 1;
  text_field.role = ax::mojom::Role::kTextField;
  text_field.AddState(ax::mojom::State::kEditable);
  text_field.AddStringAttribute(ax::mojom::StringAttribute::kValue, "Text");

  AXTreeUpdate initial_state;
  initial_state.root_id = text_field.id;
  initial_state.nodes = {text_field};

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());

  AXTreeUpdate update;
  text_field.SetInvalidState(ax::mojom::InvalidState::kTrue);
  update.nodes = {text_field};

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::INVALID_STATUS_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, AddLiveRegionAttribute) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                     "polite");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::LIVE_STATUS_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_CREATED, 1)));

  event_generator.ClearEvents();
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                     "assertive");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::LIVE_STATUS_CHANGED, 1)));

  event_generator.ClearEvents();
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                     "off");

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::LIVE_STATUS_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, CheckedStateChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kCheckBox;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[0].SetCheckedState(ax::mojom::CheckedState::kTrue);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::CHECKED_STATE_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         1)));
}

TEST(AXEventGeneratorTest, ActiveDescendantChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kListBox;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kListBoxOption;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kListBoxOption;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[0].int_attributes.clear();
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                  3);
  event_generator.RegisterEventOnNode(
      AXEventGenerator::Event::RELATED_NODE_CHANGED, update.nodes[0].id);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, ActiveDescendantChangedAndNewNodeSelection) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kGrid;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kCell;
  initial_state.nodes[1].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                          true);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kCell;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes.resize(4);
  update.nodes[0].int_attributes.clear();
  update.nodes[0].child_ids.push_back(4);
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                  4);
  update.nodes[1].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);
  update.nodes[3].id = 4;
  update.nodes[3].role = ax::mojom::Role::kCell;
  update.nodes[3].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::SELECTED_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         2),
          HasEventAtNode(AXEventGenerator::Event::SELECTED_CHANGED, 4),
          HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 4),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         4)));
}

TEST(AXEventGeneratorTest, ActiveDescendantChangedAndNewNodeSelectionIndirect) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kGrid;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[0].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kCell;
  initial_state.nodes[2].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected,
                                          true);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].child_ids.push_back(5);
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kCell;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes.resize(7);
  update.nodes[0].int_attributes.clear();
  update.nodes[0].child_ids.push_back(6);
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                  7);
  update.nodes[2].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, false);
  update.nodes[5].id = 6;
  update.nodes[5].child_ids.push_back(7);
  update.nodes[6].id = 7;
  update.nodes[6].role = ax::mojom::Role::kCell;
  update.nodes[6].AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, true);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::SELECTED_CHILDREN_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::SELECTED_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         3),
          HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 6),
          HasEventAtNode(AXEventGenerator::Event::SELECTED_CHANGED, 7),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         7)));
}

TEST(AXEventGeneratorTest, CreateAlertAndLiveRegion) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes.resize(4);
  update.nodes[0].child_ids.push_back(2);
  update.nodes[0].child_ids.push_back(3);
  update.nodes[0].child_ids.push_back(4);

  update.nodes[1].id = 2;
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                     "polite");

  // Blink should automatically add aria-live="assertive" to elements with role
  // kAlert, but we should fire an alert event regardless.
  update.nodes[2].id = 3;
  update.nodes[2].role = ax::mojom::Role::kAlert;

  // Elements with role kAlertDialog will *not* usually have a live region
  // status, but again, we should always fire an alert event.
  update.nodes[3].id = 4;
  update.nodes[3].role = ax::mojom::Role::kAlertDialog;

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::ALERT, 3),
          HasEventAtNode(AXEventGenerator::Event::ALERT, 4),
          HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_CREATED, 2),
          HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 2),
          HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 3),
          HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 4)));
}

TEST(AXEventGeneratorTest, LiveRegionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[1].string_attributes.clear();
  update.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "After 1");
  update.nodes[2].string_attributes.clear();
  update.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "After 2");

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::NAME_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::NAME_CHANGED, 3)));
}

TEST(AXEventGeneratorTest, LiveRegionOnlyTextChanges) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                     "Description 1");
  update.nodes[2].SetCheckedState(ax::mojom::CheckedState::kTrue);

  // Note that we do NOT expect a LIVE_REGION_CHANGED event here, because
  // the name did not change.
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::CHECKED_STATE_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         3),
          HasEventAtNode(AXEventGenerator::Event::DESCRIPTION_CHANGED, 2)));
}

TEST(AXEventGeneratorTest, BusyLiveRegionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kBusy,
                                          true);
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[1].string_attributes.clear();
  update.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "After 1");
  update.nodes[2].string_attributes.clear();
  update.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  update.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "After 2");

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::NAME_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::NAME_CHANGED, 3)));
}

TEST(AXEventGeneratorTest, RemoveAriaLiveOffFromChild) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[1].id = 2;
  initial_state.nodes[0].child_ids = {2};
  initial_state.nodes[0].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "History");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "New message");
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "off");
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());

  AXTreeUpdate update = initial_state;
  update.nodes[1].RemoveStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::LIVE_STATUS_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_CREATED, 2)));
}

TEST(AXEventGeneratorTest, AddChild) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes.resize(3);
  update.nodes[0].child_ids.push_back(3);
  update.nodes[2].id = 3;

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 3)));
}

TEST(AXEventGeneratorTest, RemoveChild) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes.resize(2);
  update.nodes[0].child_ids.clear();
  update.nodes[0].child_ids.push_back(2);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::CHILDREN_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, ReorderChildren) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[0].child_ids.clear();
  update.nodes[0].child_ids.push_back(3);
  update.nodes[0].child_ids.push_back(2);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::CHILDREN_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, ScrollHorizontalPositionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kScrollX, 10);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(HasEventAtNode(
          AXEventGenerator::Event::SCROLL_HORIZONTAL_POSITION_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, ScrollVerticalPositionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kScrollY, 10);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(HasEventAtNode(
          AXEventGenerator::Event::SCROLL_VERTICAL_POSITION_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, TextAttributeChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(17);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids = {2,  3,  4,  5,  6,  7,  8,  9,
                                      10, 11, 12, 13, 14, 15, 16, 17};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[4].id = 5;
  initial_state.nodes[5].id = 6;
  initial_state.nodes[6].id = 7;
  initial_state.nodes[7].id = 8;
  initial_state.nodes[8].id = 9;
  initial_state.nodes[9].id = 10;
  initial_state.nodes[10].id = 11;
  initial_state.nodes[11].id = 12;
  initial_state.nodes[12].id = 13;
  initial_state.nodes[13].id = 14;
  initial_state.nodes[14].id = 15;
  initial_state.nodes[15].id = 16;
  initial_state.nodes[16].id = 17;
  // Text attribute changes are only fired in richly editable areas.
  for (int count = 1; count <= 16; ++count) {
    initial_state.nodes[count].AddState(ax::mojom::State::kRichlyEditable);
  }

  // To test changing the start and end of existing markers.
  initial_state.nodes[11].AddIntListAttribute(
      ax::mojom::IntListAttribute::kMarkerTypes,
      {static_cast<int32_t>(ax::mojom::MarkerType::kTextMatch)});
  initial_state.nodes[11].AddIntListAttribute(
      ax::mojom::IntListAttribute::kMarkerStarts, {5});
  initial_state.nodes[11].AddIntListAttribute(
      ax::mojom::IntListAttribute::kMarkerEnds, {10});

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  // 0 is the default int attribute value, so does not generate an event.
  // (id=2).
  update.nodes[1].AddIntAttribute(ax::mojom::IntAttribute::kColor, 0);
  // 0 is the default int attribute value, so does not generate an event.
  // (id=3).
  update.nodes[2].AddIntAttribute(ax::mojom::IntAttribute::kBackgroundColor, 0);
  update.nodes[3].AddIntAttribute(
      ax::mojom::IntAttribute::kTextDirection,
      static_cast<int32_t>(ax::mojom::WritingDirection::kRtl));
  update.nodes[4].AddIntAttribute(
      ax::mojom::IntAttribute::kTextPosition,
      static_cast<int32_t>(ax::mojom::TextPosition::kSuperscript));
  update.nodes[5].AddIntAttribute(
      ax::mojom::IntAttribute::kTextStyle,
      static_cast<int32_t>(ax::mojom::TextStyle::kBold));
  update.nodes[6].AddIntAttribute(
      ax::mojom::IntAttribute::kTextOverlineStyle,
      static_cast<int32_t>(ax::mojom::TextDecorationStyle::kSolid));
  update.nodes[7].AddIntAttribute(
      ax::mojom::IntAttribute::kTextStrikethroughStyle,
      static_cast<int32_t>(ax::mojom::TextDecorationStyle::kWavy));
  update.nodes[8].AddIntAttribute(
      ax::mojom::IntAttribute::kTextUnderlineStyle,
      static_cast<int32_t>(ax::mojom::TextDecorationStyle::kDotted));
  update.nodes[9].AddIntListAttribute(
      ax::mojom::IntListAttribute::kMarkerTypes,
      {static_cast<int32_t>(ax::mojom::MarkerType::kSpelling)});
  update.nodes[10].AddIntListAttribute(
      ax::mojom::IntListAttribute::kMarkerTypes,
      {static_cast<int32_t>(ax::mojom::MarkerType::kGrammar)});
  update.nodes[11].AddIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds,
                                       {11});
  update.nodes[12].AddIntListAttribute(
      ax::mojom::IntListAttribute::kMarkerTypes,
      {static_cast<int32_t>(ax::mojom::MarkerType::kActiveSuggestion)});
  update.nodes[13].AddIntListAttribute(
      ax::mojom::IntListAttribute::kMarkerTypes,
      {static_cast<int32_t>(ax::mojom::MarkerType::kSuggestion)});
  update.nodes[14].AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize,
                                     12.0f);
  update.nodes[15].AddFloatAttribute(ax::mojom::FloatAttribute::kFontWeight,
                                     600.0f);
  update.nodes[16].AddStringAttribute(ax::mojom::StringAttribute::kFontFamily,
                                      "sans");

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 4),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 5),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 6),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 7),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 8),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 9),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 10),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 11),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 12),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 13),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 14),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 15),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 16),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 17)));
}

TEST(AXEventGeneratorTest, ObjectAttributeChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids = {2, 3};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  // Text attribute changes are only fired in richly editable areas.
  initial_state.nodes[1].AddState(ax::mojom::State::kRichlyEditable);
  initial_state.nodes[2].AddState(ax::mojom::State::kRichlyEditable);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddIntAttribute(ax::mojom::IntAttribute::kTextAlign, 2);
  update.nodes[2].AddFloatAttribute(ax::mojom::FloatAttribute::kTextIndent,
                                    10.0f);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(
              AXEventGenerator::Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED, 2),
          HasEventAtNode(
              AXEventGenerator::Event::ATK_TEXT_OBJECT_ATTRIBUTE_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::OBJECT_ATTRIBUTE_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::OBJECT_ATTRIBUTE_CHANGED,
                         3)));
}

// Note: we no longer fire OTHER_ATTRIBUTE_CHANGED for general attributes.
// We only fire specific events.
TEST(AXEventGeneratorTest, OtherAttributeChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(6);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].child_ids.push_back(4);
  initial_state.nodes[0].child_ids.push_back(5);
  initial_state.nodes[0].child_ids.push_back(6);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  // Font size attribute changes are only fired in richly editable areas.
  initial_state.nodes[3].AddState(ax::mojom::State::kRichlyEditable);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[4].id = 5;
  initial_state.nodes[5].id = 6;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                     "de");
  update.nodes[2].AddIntAttribute(ax::mojom::IntAttribute::kAriaCellColumnIndex,
                                  7);
  update.nodes[3].AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize,
                                    12.0f);
  update.nodes[4].AddBoolAttribute(ax::mojom::BoolAttribute::kModal, true);
  std::vector<int> ids = {2};
  update.nodes[5].AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds,
                                      ids);
  event_generator.RegisterEventOnNode(
      AXEventGenerator::Event::RELATED_NODE_CHANGED, update.nodes[0].id);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::CONTROLS_CHANGED, 6),
          HasEventAtNode(AXEventGenerator::Event::LANGUAGE_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::TEXT_ATTRIBUTE_CHANGED, 4),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 6)));
}

TEST(AXEventGeneratorTest, NameChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "Hello");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator, UnorderedElementsAre(HasEventAtNode(
                                   AXEventGenerator::Event::NAME_CHANGED, 2)));
}

TEST(AXEventGeneratorTest, DescriptionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kDescription,
                                     "Hello");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::DESCRIPTION_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, RoleChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[0].role = ax::mojom::Role::kCheckBox;
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator, UnorderedElementsAre(HasEventAtNode(
                                   AXEventGenerator::Event::ROLE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, MenuItemSelected) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kMenu;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[0].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kMenuListOption;
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kMenuListOption;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[0].int_attributes.clear();
  update.nodes[0].AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                  3);
  event_generator.RegisterEventOnNode(
      AXEventGenerator::Event::RELATED_NODE_CHANGED, update.nodes[0].id);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::MENU_ITEM_SELECTED, 3),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, NodeBecomesIgnored) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[2].child_ids.push_back(4);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[3].child_ids.push_back(5);
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kStaticText;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[3].AddState(ax::mojom::State::kIgnored);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 4),
                  HasEventAtNode(AXEventGenerator::Event::PARENT_CHANGED, 5)));
}

TEST(AXEventGeneratorTest, NodeBecomesIgnored2) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[2].child_ids.push_back(4);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[3].child_ids.push_back(5);
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kStaticText;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  // Marking as ignored should fire CHILDREN_CHANGED on 2
  update.nodes[3].AddState(ax::mojom::State::kIgnored);
  // Remove node id 5 so it also fires CHILDREN_CHANGED on 4.
  update.nodes.pop_back();
  update.nodes[3].child_ids.clear();
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 4)));
}

TEST(AXEventGeneratorTest, NodeBecomesUnignored) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[2].child_ids.push_back(4);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[3].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[3].child_ids.push_back(5);
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kStaticText;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[3].state = 0;
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 4),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 4),
                  HasEventAtNode(AXEventGenerator::Event::PARENT_CHANGED, 5)));
}

TEST(AXEventGeneratorTest, NodeBecomesUnignored2) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[2].child_ids.push_back(4);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[3].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[3].child_ids.push_back(5);
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kStaticText;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  // Marking as no longer ignored should fire CHILDREN_CHANGED on 2
  update.nodes[3].state = 0;
  // Remove node id 5 so it also fires CHILDREN_CHANGED on 4.
  update.nodes.pop_back();
  update.nodes[3].child_ids.clear();
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 4),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 4)));
}

TEST(AXEventGeneratorTest, NodeInsertedViaRoleChange) {
  // This test inserts a kSearch in between the kRootWebArea and the kTextField,
  // but the node id are updated reflecting position in the tree. This results
  // in node 2's role changing along with node 3 being created and added as a
  // child of node 2.
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kTextField;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update;
  update.root_id = 1;
  update.nodes.resize(3);
  update.nodes[0].id = 1;
  update.nodes[0].role = ax::mojom::Role::kRootWebArea;
  update.nodes[0].child_ids.push_back(2);
  update.nodes[1].id = 2;
  update.nodes[1].role = ax::mojom::Role::kSearch;
  update.nodes[1].child_ids.push_back(3);
  update.nodes[2].id = 3;
  update.nodes[2].role = ax::mojom::Role::kTextField;
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 3),
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::ROLE_CHANGED, 2)));
}

TEST(AXEventGeneratorTest, NodeInserted) {
  // This test inserts a kSearch in between the kRootWebArea and the kTextField.
  // The node ids reflect the creation order, and the kTextField is not changed.
  // Thus this is more like a reparenting.
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kTextField;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update;
  update.root_id = 1;
  update.nodes.resize(3);
  update.nodes[0].id = 1;
  update.nodes[0].role = ax::mojom::Role::kRootWebArea;
  update.nodes[0].child_ids.push_back(3);
  update.nodes[1].id = 3;
  update.nodes[1].role = ax::mojom::Role::kSearch;
  update.nodes[1].child_ids.push_back(2);
  update.nodes[2].id = 2;
  update.nodes[2].role = ax::mojom::Role::kTextField;
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 3),
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
                  HasEventAtNode(AXEventGenerator::Event::PARENT_CHANGED, 2)));
}

TEST(AXEventGeneratorTest, SubtreeBecomesUnignored) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[1].RemoveState(ax::mojom::State::kIgnored);
  update.nodes[2].RemoveState(ax::mojom::State::kIgnored);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 2)));
}

TEST(AXEventGeneratorTest, TwoNodesSwapIgnored) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddState(ax::mojom::State::kIgnored);
  update.nodes[2].RemoveState(ax::mojom::State::kIgnored);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 3),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 3)));
}

TEST(AXEventGeneratorTest, TwoNodesSwapIgnored2) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kArticle;
  initial_state.nodes[1].AddState(ax::mojom::State::kIgnored);
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[1].RemoveState(ax::mojom::State::kIgnored);
  update.nodes[2].AddState(ax::mojom::State::kIgnored);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 3),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 2)));
}

TEST(AXEventGeneratorTest, IgnoredChangedFiredOnAncestorOnly2) {
  // BEFORE
  //   1
  //   |
  //   2
  //  / \
  // 3   4 (IGN)

  // AFTER
  //   1
  //   |
  //   2 ___
  //  /      \
  // 3 (IGN)  4
  // IGNORED_CHANGED expected on #3, #4

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids = {2};

  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kGroup;
  initial_state.nodes[1].child_ids = {3, 4};

  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;

  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[3].AddState(ax::mojom::State::kIgnored);

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[2].AddState(ax::mojom::State::kIgnored);
  update.nodes[3].RemoveState(ax::mojom::State::kIgnored);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 3),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 4),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 4)));
}

TEST(AXEventGeneratorTest, IgnoredChangedFiredOnAncestorOnly8) {
  // BEFORE
  //         ____ 1 ____
  //       |            |
  //       2 (IGN)      7
  //       |
  //       3 (IGN)
  //       |
  //       4 (IGN)
  //       |
  //       5 (IGN)
  //       |
  //       6 (IGN)

  // AFTER
  //         ____ 1 ____
  //       |            |
  //       2            7 (IGN)
  //       |
  //       3
  //       |
  //       4
  //       |
  //       5
  //       |
  //       6

  // IGNORED_CHANGED expected on #2, #7

  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(7);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids = {2, 7};

  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kGroup;
  initial_state.nodes[1].child_ids = {3};
  initial_state.nodes[1].AddState(ax::mojom::State::kIgnored);

  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].child_ids = {4};
  initial_state.nodes[2].AddState(ax::mojom::State::kIgnored);

  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[3].child_ids = {5};
  initial_state.nodes[3].AddState(ax::mojom::State::kIgnored);

  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kGroup;
  initial_state.nodes[4].child_ids = {6};
  initial_state.nodes[4].AddState(ax::mojom::State::kIgnored);

  initial_state.nodes[5].id = 5;
  initial_state.nodes[5].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[5].AddState(ax::mojom::State::kIgnored);

  initial_state.nodes[6].id = 7;
  initial_state.nodes[6].role = ax::mojom::Role::kStaticText;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[1].RemoveState(ax::mojom::State::kIgnored);
  update.nodes[2].RemoveState(ax::mojom::State::kIgnored);
  update.nodes[3].RemoveState(ax::mojom::State::kIgnored);
  update.nodes[4].RemoveState(ax::mojom::State::kIgnored);
  update.nodes[5].RemoveState(ax::mojom::State::kIgnored);
  update.nodes[6].AddState(ax::mojom::State::kIgnored);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 2),
                  HasEventAtNode(AXEventGenerator::Event::IGNORED_CHANGED, 7)));
}

TEST(AXEventGeneratorTest, ActiveDescendantChangeOnDescendant) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kRootWebArea;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGroup;
  initial_state.nodes[2].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 4);
  initial_state.nodes[2].child_ids.push_back(4);
  initial_state.nodes[2].child_ids.push_back(5);
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGroup;
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kGroup;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  initial_state.nodes[2].RemoveIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId);
  initial_state.nodes[2].AddIntAttribute(
      ax::mojom::IntAttribute::kActivedescendantId, 5);
  AXTreeUpdate update = initial_state;
  // Setting the node_id_to_clear causes AXTree::ComputePendingChangesToNode to
  // create all of the node's children. Since node 3 already exists and remains
  // in the tree, that (re)created child is reporting a new parent.
  update.node_id_to_clear = 2;
  event_generator.RegisterEventOnNode(
      AXEventGenerator::Event::RELATED_NODE_CHANGED, update.nodes[0].id);
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::ACTIVE_DESCENDANT_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::PARENT_CHANGED, 3)));
}

TEST(AXEventGeneratorTest, ImageAnnotationChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kImageAnnotation, "Hello");
  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, ImageAnnotationStatusChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[0].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::IMAGE_ANNOTATION_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, StringPropertyChanges) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  struct {
    ax::mojom::StringAttribute id;
    std::string old_value;
    std::string new_value;
  } attributes[] = {
      {ax::mojom::StringAttribute::kAccessKey, "a", "b"},
      {ax::mojom::StringAttribute::kClassName, "a", "b"},
      {ax::mojom::StringAttribute::kKeyShortcuts, "a", "b"},
      {ax::mojom::StringAttribute::kLanguage, "a", "b"},
      {ax::mojom::StringAttribute::kPlaceholder, "a", "b"},
  };
  for (auto&& attrib : attributes) {
    initial_state.nodes.emplace_back();
    initial_state.nodes.back().id = initial_state.nodes.size();
    initial_state.nodes.back().AddStringAttribute(attrib.id, attrib.old_value);
    initial_state.nodes[0].child_ids.push_back(initial_state.nodes.size());
  }

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  int index = 1;
  for (auto&& attrib : attributes) {
    initial_state.nodes[index++].AddStringAttribute(attrib.id,
                                                    attrib.new_value);
  }

  AXTreeUpdate update = initial_state;
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::ACCESS_KEY_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::KEY_SHORTCUTS_CHANGED, 4),
          HasEventAtNode(AXEventGenerator::Event::LANGUAGE_CHANGED, 5),
          HasEventAtNode(AXEventGenerator::Event::PLACEHOLDER_CHANGED, 6)));
}

TEST(AXEventGeneratorTest, IntPropertyChanges) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  struct {
    ax::mojom::IntAttribute id;
    int old_value;
    int new_value;
  } attributes[] = {
      {ax::mojom::IntAttribute::kHierarchicalLevel, 1, 2},
      {ax::mojom::IntAttribute::kPosInSet, 1, 2},
      {ax::mojom::IntAttribute::kSetSize, 1, 2},
  };
  for (auto&& attrib : attributes) {
    initial_state.nodes.emplace_back();
    initial_state.nodes.back().id = initial_state.nodes.size();
    initial_state.nodes.back().AddIntAttribute(attrib.id, attrib.old_value);
    initial_state.nodes[0].child_ids.push_back(initial_state.nodes.size());
  }

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  int index = 1;
  for (auto&& attrib : attributes)
    initial_state.nodes[index++].AddIntAttribute(attrib.id, attrib.new_value);

  AXTreeUpdate update = initial_state;
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::HIERARCHICAL_LEVEL_CHANGED,
                         2),
          HasEventAtNode(AXEventGenerator::Event::POSITION_IN_SET_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::SET_SIZE_CHANGED, 4)));
}

TEST(AXEventGeneratorTest, IntListPropertyChanges) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  struct {
    ax::mojom::IntListAttribute id;
    std::vector<int> old_value;
    std::vector<int> new_value;
  } attributes[] = {
      {ax::mojom::IntListAttribute::kDescribedbyIds, {1}, {2}},
      {ax::mojom::IntListAttribute::kFlowtoIds, {1}, {2}},
      {ax::mojom::IntListAttribute::kLabelledbyIds, {1}, {2}},
  };
  for (auto&& attrib : attributes) {
    initial_state.nodes.emplace_back();
    initial_state.nodes.back().id = initial_state.nodes.size();
    initial_state.nodes.back().AddIntListAttribute(attrib.id, attrib.old_value);
    initial_state.nodes[0].child_ids.push_back(initial_state.nodes.size());
  }

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  int index = 1;
  for (auto&& attrib : attributes) {
    initial_state.nodes[index++].AddIntListAttribute(attrib.id,
                                                     attrib.new_value);
  }

  AXTreeUpdate update = initial_state;
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::DESCRIBED_BY_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::FLOW_FROM_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::FLOW_FROM_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::FLOW_TO_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::LABELED_BY_CHANGED, 4)
          // Related node changed not fired because it requires explicit
          // registration.
          ));
}

TEST(AXEventGeneratorTest, AriaBusyChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kBusy,
                                          true);
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kBusy, false);

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::BUSY_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::LAYOUT_INVALIDATED, 1),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         1)));
}

TEST(AXEventGeneratorTest, MultiselectableStateChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kGrid;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddState(ax::mojom::State::kMultiselectable);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::MULTISELECTABLE_STATE_CHANGED,
                         1),
          HasEventAtNode(AXEventGenerator::Event::STATE_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         1)));
}

TEST(AXEventGeneratorTest, RequiredStateChanged) {
  AXNodeData text_field;
  text_field.id = 1;
  text_field.role = ax::mojom::Role::kTextField;
  text_field.AddState(ax::mojom::State::kEditable);

  AXTreeUpdate initial_state;
  initial_state.root_id = text_field.id;
  initial_state.nodes = {text_field};

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());

  AXTreeUpdate update;
  text_field.AddState(ax::mojom::State::kRequired);
  update.nodes = {text_field};

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::REQUIRED_STATE_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::STATE_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         1)));
}

TEST(AXEventGeneratorTest, FlowToChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(6);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[0].child_ids.assign({2, 3, 4, 5, 6});
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[1].AddIntListAttribute(
      ax::mojom::IntListAttribute::kFlowtoIds, {3, 4});
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[5].id = 6;
  initial_state.nodes[5].role = ax::mojom::Role::kGenericContainer;

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;
  update.nodes[1].AddIntListAttribute(ax::mojom::IntListAttribute::kFlowtoIds,
                                      {4, 5, 6});
  event_generator.RegisterEventOnNode(
      AXEventGenerator::Event::RELATED_NODE_CHANGED, update.nodes[0].id);

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::FLOW_FROM_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::FLOW_FROM_CHANGED, 5),
          HasEventAtNode(AXEventGenerator::Event::FLOW_FROM_CHANGED, 6),
          HasEventAtNode(AXEventGenerator::Event::FLOW_TO_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 2)));
}

TEST(AXEventGeneratorTest, ControlsChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(2);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;

  std::vector<int> ids = {2};
  update.nodes[0].AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds,
                                      ids);
  event_generator.RegisterEventOnNode(
      AXEventGenerator::Event::RELATED_NODE_CHANGED, update.nodes[0].id);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::CONTROLS_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::RELATED_NODE_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, AtomicChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic, true);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::ATOMIC_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, HasPopupChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;

  update.nodes[0].SetHasPopup(ax::mojom::HasPopup::kTrue);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::HASPOPUP_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         1)));
}

TEST(AXEventGeneratorTest, LiveRelevantChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddStringAttribute(ax::mojom::StringAttribute::kLiveRelevant,
                                     "all");
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::LIVE_RELEVANT_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, MultilineStateChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());
  AXTreeUpdate update = initial_state;

  update.nodes[0].AddState(ax::mojom::State::kMultiline);
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::MULTILINE_STATE_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::STATE_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::WIN_IACCESSIBLE_STATE_CHANGED,
                         1)));
}

TEST(AXEventGeneratorTest, EditableTextChanged) {
  AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;

  AXNodeData text_field;
  text_field.id = 2;
  text_field.role = ax::mojom::Role::kTextField;
  text_field.AddState(ax::mojom::State::kEditable);
  text_field.SetValue("Before");
  root.child_ids = {text_field.id};

  AXNodeData static_text;
  static_text.id = 3;
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.AddState(ax::mojom::State::kEditable);
  static_text.SetName("Before");
  text_field.child_ids = {static_text.id};

  AXTreeUpdate initial_state;
  initial_state.root_id = root.id;
  initial_state.nodes = {root, text_field, static_text};
  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());

  text_field.SetValue("After");
  static_text.SetName("After");
  AXTreeUpdate update;
  update.nodes = {text_field, static_text};

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::VALUE_IN_TEXT_FIELD_CHANGED,
                         text_field.id),
          HasEventAtNode(AXEventGenerator::Event::NAME_CHANGED, static_text.id),
          HasEventAtNode(AXEventGenerator::Event::EDITABLE_TEXT_CHANGED,
                         text_field.id)));
}

TEST(AXEventGeneratorTest, CheckedStateDescriptionChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());

  AXTreeUpdate update = initial_state;
  update.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kCheckedStateDescription, "Checked");
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(HasEventAtNode(
          AXEventGenerator::Event::CHECKED_STATE_DESCRIPTION_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, LiveRegionNodeRemovedNotRelevant) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[0].child_ids = {2, 3};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(1);
  update.nodes[0].child_ids = {2};

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::CHILDREN_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, LiveRegionNodeRemovedAllRelevant) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveRelevant, "all");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant, "all");
  initial_state.nodes[0].child_ids = {2, 3};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant, "all");
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant, "all");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(1);
  update.nodes[0].child_ids = {2};

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, LiveRegionNodeRemovedAdditionsRelevant) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveRelevant, "additions");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant, "additions");
  initial_state.nodes[0].child_ids = {2, 3};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant, "additions");
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant, "additions");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(1);
  update.nodes[0].child_ids = {2};

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::CHILDREN_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, LiveRegionNodeRemovedRemovalsRelevant) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveRelevant, "removals");
  initial_state.nodes[0].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant, "removals");
  initial_state.nodes[0].child_ids = {2, 3};
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant, "removals");
  initial_state.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 1");
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant, "removals");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Before 2");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(1);
  update.nodes[0].child_ids = {2};

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, LiveRegionNodeReparentedAdditionsRelevant) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(5);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids = {2, 3};

  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids = {4};
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");

  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");

  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].child_ids = {5};
  initial_state.nodes[3].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[3].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Live child");

  initial_state.nodes[4].id = 5;
  initial_state.nodes[4].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[4].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[4].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Live child");

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[1].child_ids = {};
  update.nodes[2].child_ids = {4};

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::PARENT_CHANGED, 4),
          HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 2),
          HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 3)));

  update.nodes[4].string_attributes.clear();
  update.nodes[4].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  update.nodes[4].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "Live child after");
  event_generator.ClearEvents();
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED, 5),
          HasEventAtNode(AXEventGenerator::Event::NAME_CHANGED, 5)));
}

TEST(AXEventGeneratorTest, LiveRegionRootRemoved) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids = {2};

  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].role = ax::mojom::Role::kGenericContainer;
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[1].child_ids = {3};
  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].role = ax::mojom::Role::kStaticText;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Live");
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes.resize(1);
  update.nodes[0].child_ids = {};

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(HasEventAtNode(
                  AXEventGenerator::Event::CHILDREN_CHANGED, 1)));
}

TEST(AXEventGeneratorTest, LiveRootsNested) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids = {2};

  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids = {3};
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[1].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");

  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].child_ids = {4};

  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[3].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Live child");

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;

  update.nodes[3].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                     "Live child after");

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_NODE_CHANGED, 4),
          HasEventAtNode(AXEventGenerator::Event::NAME_CHANGED, 4)));
}

TEST(AXEventGeneratorTest, LiveRootDescendantOfClearedNodeChanged) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  initial_state.nodes.resize(4);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].child_ids = {2};

  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids = {3};

  initial_state.nodes[2].id = 3;
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kLiveRelevant, "additions removals");
  initial_state.nodes[2].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant, "additions removals");
  initial_state.nodes[2].child_ids = {4};

  initial_state.nodes[3].id = 4;
  initial_state.nodes[3].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  initial_state.nodes[3].AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant, "additions removals");
  initial_state.nodes[3].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                            "Live child");

  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update = initial_state;
  update.nodes[2].child_ids = {};
  update.nodes.resize(3);

  // In this case the live region root is "reparented" because its removed
  // when its parent is cleared and then re-added in the update.
  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::LIVE_REGION_CHANGED, 3),
          HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 3)));
}

TEST(AXEventGeneratorTest, NoParentChangedOnIgnoredNode) {
  // This test produces parent-changed events on ignored nodes, and serves as a
  // way to test they are properly removed in PostprocessEvents.
  // It was created through pseudo-automatic code generation and is based on the
  // chrome://history page, where we detected this kind of events happening.

  // BEFORE:
  // id=47 application child_ids=167,94
  //   id=167 inlineTextBox
  //   id=94 grid child_ids=98,99
  //     id=98 genericContainer IGNORED
  //     id=99 genericContainer child_ids=100
  //       id=100 genericContainer IGNORED child_ids=101
  //         id=101 genericContainer IGNORED INVISIBLE

  // AFTER
  // id=47 application child_ids=167,168
  //   id=167 inlineTextBox
  //   id=168 grid IGNORED INVISIBLE child_ids=169,170
  //     id=169 genericContainer IGNORED INVISIBLE
  //     id=170 genericContainer IGNORED INVISIBLE child_ids=100
  //       id=100 genericContainer IGNORED INVISIBLE child_ids=101
  //         id=101 genericContainer IGNORED INVISIBLE

  AXTreeUpdate initial_state;
  initial_state.root_id = 47;
  {
    AXNodeData data;
    data.id = 47;
    data.role = ax::mojom::Role::kApplication;
    data.child_ids = {167, 94};
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 167;
    data.role = ax::mojom::Role::kInlineTextBox;
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 94;
    data.role = ax::mojom::Role::kGrid;
    data.child_ids = {98, 99};
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 98;
    data.role = ax::mojom::Role::kGenericContainer;
    data.AddState(ax::mojom::State::kIgnored);
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 99;
    data.role = ax::mojom::Role::kGenericContainer;
    data.child_ids = {100};
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 100;
    data.role = ax::mojom::Role::kGenericContainer;
    data.child_ids = {101};
    data.AddState(ax::mojom::State::kIgnored);
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 101;
    data.role = ax::mojom::Role::kGenericContainer;
    data.AddState(ax::mojom::State::kInvisible);
    data.AddState(ax::mojom::State::kIgnored);
    initial_state.nodes.push_back(data);
  }

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());

  AXTreeUpdate update;
  {
    AXNodeData data;
    data.id = 47;
    data.role = ax::mojom::Role::kApplication;
    data.child_ids = {167, 168};
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 167;
    data.role = ax::mojom::Role::kInlineTextBox;
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 168;
    data.role = ax::mojom::Role::kGrid;
    data.child_ids = {169, 170};
    data.AddState(ax::mojom::State::kInvisible);
    data.AddState(ax::mojom::State::kIgnored);
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 169;
    data.role = ax::mojom::Role::kGenericContainer;
    data.AddState(ax::mojom::State::kInvisible);
    data.AddState(ax::mojom::State::kIgnored);
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 170;
    data.role = ax::mojom::Role::kGenericContainer;
    data.child_ids = {100};
    data.AddState(ax::mojom::State::kInvisible);
    data.AddState(ax::mojom::State::kIgnored);
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 100;
    data.role = ax::mojom::Role::kGenericContainer;
    data.child_ids = {101};
    data.AddState(ax::mojom::State::kInvisible);
    data.AddState(ax::mojom::State::kIgnored);
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 101;
    data.role = ax::mojom::Role::kGenericContainer;
    data.AddState(ax::mojom::State::kInvisible);
    data.AddState(ax::mojom::State::kIgnored);
    update.nodes.push_back(data);
  }

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 47),
          HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 168)));
  // These are the events that shouldn't be happening:
  // HasEventAtNode(AXEventGenerator::Event::PARENT_CHANGED, 100),
  // HasEventAtNode(AXEventGenerator::Event::PARENT_CHANGED, 101),
}

TEST(AXEventGeneratorTest, ParentChangedOnIgnoredNodeFiresOnChildren) {
  // This is a variation of the previous test, designed to check if, in the
  // situation of parent-changed events happening on ignored nodes, the events
  // are correctly fired in their non-ignored children.

  // BEFORE:
  // id=47 application child_ids=167,94
  //   id=167 inlineTextBox
  //   id=94 grid child_ids=98,99
  //     id=98 genericContainer IGNORED
  //     id=99 genericContainer child_ids=100
  //       id=100 genericContainer IGNORED child_ids=101,102
  //         id=101 genericContainer IGNORED INVISIBLE child_ids=103,104
  //           id=103 staticText
  //           id=104 staticText
  //         id=102 staticText

  // AFTER
  // id=47 application child_ids=167,168
  //   id=167 inlineTextBox
  //   id=168 grid IGNORED INVISIBLE child_ids=169,170
  //     id=169 genericContainer IGNORED INVISIBLE
  //     id=170 genericContainer IGNORED INVISIBLE child_ids=100
  //       id=100 genericContainer IGNORED INVISIBLE child_ids=101,102
  //         id=101 genericContainer IGNORED INVISIBLE child_ids=103,104
  //           id=103 staticText
  //           id=104 staticText
  //         id=102 staticText

  AXTreeUpdate initial_state;
  initial_state.root_id = 47;
  {
    AXNodeData data;
    data.id = 47;
    data.role = ax::mojom::Role::kApplication;
    data.child_ids = {167, 94};
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 167;
    data.role = ax::mojom::Role::kInlineTextBox;
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 94;
    data.role = ax::mojom::Role::kGrid;
    data.child_ids = {98, 99};
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 98;
    data.role = ax::mojom::Role::kGenericContainer;
    data.AddState(ax::mojom::State::kIgnored);
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 99;
    data.role = ax::mojom::Role::kGenericContainer;
    data.child_ids = {100};
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 100;
    data.role = ax::mojom::Role::kGenericContainer;
    data.child_ids = {101, 102};
    data.AddState(ax::mojom::State::kIgnored);
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 101;
    data.role = ax::mojom::Role::kGenericContainer;
    data.child_ids = {103, 104};
    data.AddState(ax::mojom::State::kInvisible);
    data.AddState(ax::mojom::State::kIgnored);
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 102;
    data.role = ax::mojom::Role::kStaticText;
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 103;
    data.role = ax::mojom::Role::kStaticText;
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 104;
    data.role = ax::mojom::Role::kStaticText;
    initial_state.nodes.push_back(data);
  }

  AXTree tree(initial_state);
  AXEventGenerator event_generator(&tree);
  ASSERT_THAT(event_generator, IsEmpty());

  AXTreeUpdate update;
  {
    AXNodeData data;
    data.id = 47;
    data.role = ax::mojom::Role::kApplication;
    data.child_ids = {167, 168};
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 167;
    data.role = ax::mojom::Role::kInlineTextBox;
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 168;
    data.role = ax::mojom::Role::kGrid;
    data.child_ids = {169, 170};
    data.AddState(ax::mojom::State::kInvisible);
    data.AddState(ax::mojom::State::kIgnored);
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 169;
    data.role = ax::mojom::Role::kGenericContainer;
    data.AddState(ax::mojom::State::kInvisible);
    data.AddState(ax::mojom::State::kIgnored);
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 170;
    data.role = ax::mojom::Role::kGenericContainer;
    data.child_ids = {100};
    data.AddState(ax::mojom::State::kInvisible);
    data.AddState(ax::mojom::State::kIgnored);
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 100;
    data.role = ax::mojom::Role::kGenericContainer;
    data.child_ids = {101, 102};
    data.AddState(ax::mojom::State::kInvisible);
    data.AddState(ax::mojom::State::kIgnored);
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 101;
    data.role = ax::mojom::Role::kGenericContainer;
    data.child_ids = {103, 104};
    data.AddState(ax::mojom::State::kInvisible);
    data.AddState(ax::mojom::State::kIgnored);
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 102;
    data.role = ax::mojom::Role::kStaticText;
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 103;
    data.role = ax::mojom::Role::kStaticText;
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 104;
    data.role = ax::mojom::Role::kStaticText;
    update.nodes.push_back(data);
  }

  ASSERT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(
      event_generator,
      UnorderedElementsAre(
          HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 47),
          HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 168),
          HasEventAtNode(AXEventGenerator::Event::PARENT_CHANGED, 102),
          HasEventAtNode(AXEventGenerator::Event::PARENT_CHANGED, 103),
          HasEventAtNode(AXEventGenerator::Event::PARENT_CHANGED, 104)));
  // These are the events that shouldn't be happening:
  // HasEventAtNode(AXEventGenerator::Event::PARENT_CHANGED, 100),
  // HasEventAtNode(AXEventGenerator::Event::PARENT_CHANGED, 101),
}

TEST(AXEventGeneratorTest, InsertUnderIgnoredTest) {
  AXTreeUpdate initial_state;
  initial_state.root_id = 1;
  {
    AXNodeData data;
    data.id = 1;
    data.role = ax::mojom::Role::kRootWebArea;
    data.child_ids = {3};
    initial_state.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 3;
    data.role = ax::mojom::Role::kGenericContainer;
    data.AddState(ax::mojom::State::kIgnored);
    initial_state.nodes.push_back(data);
  }
  AXTree tree(initial_state);

  AXEventGenerator event_generator(&tree);
  AXTreeUpdate update;
  update.node_id_to_clear = 3;
  {
    AXNodeData data;
    data.id = 3;
    data.role = ax::mojom::Role::kGenericContainer;
    data.child_ids = {5};
    data.AddState(ax::mojom::State::kIgnored);
    update.nodes.push_back(data);
  }
  {
    AXNodeData data;
    data.id = 5;
    data.role = ax::mojom::Role::kGenericContainer;
    update.nodes.push_back(data);
  }

  EXPECT_TRUE(tree.Unserialize(update));
  EXPECT_THAT(event_generator,
              UnorderedElementsAre(
                  HasEventAtNode(AXEventGenerator::Event::CHILDREN_CHANGED, 1),
                  HasEventAtNode(AXEventGenerator::Event::SUBTREE_CREATED, 5)));
}

TEST(AXEventGeneratorTest, ParseGeneratedEvent) {
  AXEventGenerator::Event event = AXEventGenerator::Event::NONE;
  for (int i = 0; i < static_cast<int>(AXEventGenerator::Event::MAX_VALUE);
       i++) {
    const char* val = ToString(static_cast<AXEventGenerator::Event>(i));
    EXPECT_TRUE(MaybeParseGeneratedEvent(val, &event));
    EXPECT_EQ(i, static_cast<int>(event));
  }
}

TEST(AXEventGenerator, ParsingUnknownEvent) {
  AXEventGenerator::Event event = AXEventGenerator::Event::CARET_BOUNDS_CHANGED;

  // No crash.
  EXPECT_FALSE(MaybeParseGeneratedEvent("kittens", &event));

  // Event should not be changed
  EXPECT_EQ(event, AXEventGenerator::Event::CARET_BOUNDS_CHANGED);
}

}  // namespace ui
