// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/test_ax_tree_update.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"

namespace ui {

int PlusSignCount(const std::string& s) {
  int count = 0;

  for (auto& character : s) {
    if (character == '+') {
      count++;
    }
  }

  return count;
}

bool IsSpaceOrTab(const char c) {
  return c == ' ' || c == '\t';
}

void RemoveLeadingAndTrailingWhitespace(std::string& s) {
  if (s.empty()) {
    return;
  }
  auto front_it = s.begin();
  auto back_it = s.end() - 1;

  while (IsSpaceOrTab(*front_it) || IsSpaceOrTab(*back_it)) {
    if (front_it > back_it) {
      return;
    }
    if (front_it == back_it) {
      if (IsSpaceOrTab(*front_it)) {
        s.erase(front_it);
      }
      return;
    }
    if (IsSpaceOrTab(*front_it)) {
      s.erase(front_it);

      // Iterators get invalidated when erasing.
      front_it = s.begin();
      back_it = s.end() - 1;
    }
    if (IsSpaceOrTab(*back_it)) {
      s.erase(back_it);

      // Iterators get invalidated when erasing.
      back_it = s.end() - 1;
      front_it = s.begin();
    }
  }
}

bool StringToBool(const std::string& s) {
  if (s == "true" || s == "True" || s == "1") {
    return true;
  }
  if (s == "false" || s == "False" || s == "0") {
    return false;
  }

  // TODO: Specify which node this error was found at.
  NOTREACHED_IN_MIGRATION() << "Invalid value passed to StringToBool: " << s;
  return false;
}

void ParseAndAddNodeProperties(
    AXNodeData& node_data,
    const std::vector<std::string>& node_line) {
  // At this point, the vector of strings we receive should be just a vector of
  // properties of the format:
  // [<property>=<value>] where value depends on the property.

  DCHECK(node_line.size() >= 1)
      << "Error in formatting of the tree structure. Possibly extra whiespace.";
  for (auto& prop : node_line) {
    std::vector<std::string> property_vector = base::SplitStringUsingSubstr(
        prop, "=", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    DCHECK(property_vector.size() == 2)
        << "Properties should always be formed as "
           "<SupportedProperty>=<PropertyValue/s>";
    std::string property = property_vector[0];

    if (property == "name" || property == "Name") {
      // Since the structure is passed in as an unformatted string,
      // and the name has the format "<name>",
      // the string adds escape characters before the quotes.
      // Therefore when we set the name we must remove the `\"` character.

      std::string name = property_vector[1];
      DCHECK(name.front() == '\"' && name.back() == '\"');
      name.erase(name.begin());
      name.erase(--name.end());

      node_data.SetName(name);
    } else if (property == "states" || property == "state") {
      std::vector<std::string> state_values = base::SplitStringUsingSubstr(
          property_vector[1], ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      DCHECK(state_values.size() >= 1)
          << "State values should be at least one, and they should be "
             "separated by commas.";
      for (auto& value : state_values) {
        node_data.AddState(StringToState(value));
      }

    } else if (property == "intAttribute" || property == "IntAttribute" ||
               property == "intattribute") {
      std::vector<std::string> int_attribute_vector =
          base::SplitStringUsingSubstr(property_vector[1], ",",
                                       base::KEEP_WHITESPACE,
                                       base::SPLIT_WANT_ALL);
      DCHECK(int_attribute_vector.size() == 2)
          << "Int attribute in string must always be formed: "
             "intAttribute=<intAttributeType>,<intAttributeValue>";
      int int_value = 0;
      base::StringToInt(int_attribute_vector[1], &int_value);
      DCHECK(int_value) << "Formatting error or non integer passed as node ID.";
      node_data.AddIntAttribute(StringToIntAttribute(int_attribute_vector[0]),
                                int_value);

    } else if (property == "stringAttribute" || property == "StringAttribute" ||
               property == "stringattribute") {
      std::vector<std::string> string_attribute_vector =
          base::SplitStringUsingSubstr(property_vector[1], ",",
                                       base::KEEP_WHITESPACE,
                                       base::SPLIT_WANT_ALL);
      DCHECK(string_attribute_vector.size() == 2)
          << "String attribute in string must always be formed: "
             "stringAttribute=<stringAttributeType>,<stringAttributeValue>";
      node_data.AddStringAttribute(
          StringToStringAttribute(string_attribute_vector[0]),
          string_attribute_vector[1]);
    } else if (property == "boolAttribute" || property == "BoolAttribute" ||
               property == "boolattribute") {
      std::vector<std::string> bool_attribute_vector =
          base::SplitStringUsingSubstr(property_vector[1], ",",
                                       base::KEEP_WHITESPACE,
                                       base::SPLIT_WANT_ALL);
      DCHECK(bool_attribute_vector.size() == 2)
          << "Bool attribute in string must always be formed: "
             "boolAttribute=<boolAttributeType>,<boolAttributeValue>";
      node_data.AddBoolAttribute(
          StringToBoolAttribute(bool_attribute_vector[0]),
          StringToBool(bool_attribute_vector[1]));

    } else {
      // TODO: Will extend to more properties here.
      NOTREACHED_IN_MIGRATION()
          << "Either an invalid property was specified, or this function does "
             "not currently support the specified property.";
    }
  }
}

AXNodeData ParseNodeInfo(std::vector<std::string>& node_line,
                                           std::set<int>& found_ids) {
  AXNodeData data;

  // Must at the very least have id and role.
  DCHECK(node_line.size() >= 2) << "Error, a node must have an id and role.";
  int int_value = 0;
  base::StringToInt(node_line[0], &int_value);
  DCHECK(int_value) << "Formatting error or non integer passed as node ID.";
  data.id = int_value;

  DCHECK(found_ids.find(data.id) == found_ids.end())
      << "Error, can't have duplicate IDs.";
  found_ids.insert(data.id);
  ax::mojom::Role role = StringToRole(node_line[1]);

  data.role = role;

  data.child_ids = {};

  if (node_line.size() >= 3) {
    node_line.erase(node_line.begin());
    node_line.erase(node_line.begin());
    ParseAndAddNodeProperties(data, node_line);
  }

  return data;
}

TestAXTreeUpdateNode::TestAXTreeUpdateNode(const TestAXTreeUpdateNode&) =
    default;

TestAXTreeUpdateNode::TestAXTreeUpdateNode(TestAXTreeUpdateNode&&) = default;

TestAXTreeUpdateNode::~TestAXTreeUpdateNode() = default;

TestAXTreeUpdateNode::TestAXTreeUpdateNode(
    ax::mojom::Role role,
    const std::vector<TestAXTreeUpdateNode>& children)
    : children(children) {
  DCHECK_NE(role, ax::mojom::Role::kUnknown);
  data.role = role;
}

TestAXTreeUpdateNode::TestAXTreeUpdateNode(
    ax::mojom::Role role,
    ax::mojom::State state,
    const std::vector<TestAXTreeUpdateNode>& children)
    : children(children) {
  DCHECK_NE(role, ax::mojom::Role::kUnknown);
  DCHECK_NE(state, ax::mojom::State::kNone);
  data.role = role;
  data.AddState(state);
}

TestAXTreeUpdateNode::TestAXTreeUpdateNode(const std::string& text) {
  data.role = ax::mojom::Role::kStaticText;
  data.SetName(text);
}

TestAXTreeUpdate::TestAXTreeUpdate(const TestAXTreeUpdateNode& root) {
  root_id = SetSubtree(root);
}

AXNodeID TestAXTreeUpdate::SetSubtree(const TestAXTreeUpdateNode& node) {
  size_t node_index = nodes.size();
  nodes.push_back(node.data);
  nodes[node_index].id = node_index + 1;
  std::vector<AXNodeID> child_ids;
  for (const auto& child : node.children) {
    child_ids.push_back(SetSubtree(child));
  }
  nodes[node_index].child_ids = child_ids;
  return nodes[node_index].id;
}

TestAXTreeUpdate::TestAXTreeUpdate(const std::string& tree_structure) {
  std::vector<AXNodeData> node_data_vector;
  std::vector<std::string> tree_structure_vector = base::SplitStringUsingSubstr(
      tree_structure, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // Test authors might accidentally include whitespace when declaring the non
  // formatted string.
  RemoveLeadingAndTrailingWhitespace(*tree_structure_vector.begin());
  RemoveLeadingAndTrailingWhitespace(*--tree_structure_vector.end());

  // With how we pass in the non formatted string, the first and last elements
  // of the vector should be empty.
  DCHECK(tree_structure_vector.front().empty() &&
         tree_structure_vector.back().empty())
      << "Error in parsing the tree structure, double check the formatting.";
  tree_structure_vector.erase(tree_structure_vector.begin());
  tree_structure_vector.erase(--tree_structure_vector.end());

  node_data_vector.reserve(tree_structure_vector.size());

  RemoveLeadingAndTrailingWhitespace(tree_structure_vector[0]);

  int root_pluses = PlusSignCount(tree_structure_vector[0]);
  DCHECK_EQ(root_pluses, 2)
      << "The first line of the test needs to start with 2 '+' sign, not "
      << root_pluses;

  // We remove the plus signs from the string
  tree_structure_vector[0].erase(0, root_pluses);
  std::vector<std::string> root_line =
      base::SplitStringUsingSubstr(tree_structure_vector[0], " ",
                                   base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // Set to keep track of found IDs and make sure we don't have duplicates.
  std::set<int> found_ids;

  node_data_vector.push_back(ParseNodeInfo(root_line, found_ids));

  // This maps the number of plus signs to the last index that number of plus
  // signs was found at in the `tree_structure_vector` This will be useful for
  // determining descendants.
  std::unordered_map<int, int> last_index_appearance_of_plus_count;

  // The first (zeroth) row always has two plus signs
  last_index_appearance_of_plus_count[2] = 0;
  int last_count = 2;
  int greatest_count = 2;
  for (size_t i = 1; i < tree_structure_vector.size(); i++) {
    // Test authors might have added whitespace when passing in the tree
    // structure
    RemoveLeadingAndTrailingWhitespace(tree_structure_vector[i]);

    int plus_count = PlusSignCount(tree_structure_vector[i]);
    DCHECK(plus_count % 2 == 0)
        << "Error in plus sign count, can't have an odd number of plus signs";
    DCHECK(plus_count <= greatest_count + 2)
        << "Error in plus signs count at tree structure line number " << i
        << ", it can't have more than two more plus signs than the previous "
           "element.";

    if (plus_count > greatest_count) {
      greatest_count = plus_count;
    }

    auto elem = last_index_appearance_of_plus_count.find(plus_count);
    if (elem == last_index_appearance_of_plus_count.end() ||
        (int)i > elem->second) {
      last_index_appearance_of_plus_count[plus_count] = i;
    }

    // We remove the plus signs from the string.
    tree_structure_vector[i].erase(0, plus_count);
    std::vector<std::string> node_line = base::SplitStringUsingSubstr(
        tree_structure_vector[i], " ", base::KEEP_WHITESPACE,
        base::SPLIT_WANT_ALL);

    AXNodeData curr_node = ParseNodeInfo(node_line, found_ids);
    node_data_vector.push_back(curr_node);

    // Two cases. Either This line's plus count is greater than the last line's,
    // which would make `curr_node` the direct child of the last node. Or the
    // current plus count is <= last one, which would mean the parent is higher
    // up.
    if (plus_count > last_count) {
      node_data_vector[i - 1].child_ids.push_back(curr_node.id);
    } else {
      elem = last_index_appearance_of_plus_count.find(plus_count - 2);
      CHECK(elem != last_index_appearance_of_plus_count.end())
          << "Error in plus sign count.";
      int parent_index = elem->second;
      node_data_vector[parent_index].child_ids.push_back(curr_node.id);
    }

    last_count = plus_count;
  }

  DCHECK(node_data_vector.size() >= 1);

  tree_data.tree_id = AXTreeID::CreateNewAXTreeID();
  tree_data.focused_tree_id = tree_data.tree_id;
  tree_data.parent_tree_id = AXTreeIDUnknown();
  tree_data = tree_data;
  has_tree_data = true;
  root_id = node_data_vector[0].id;

  for (auto& node : node_data_vector) {
    nodes.push_back(node);
  }
}

}  // namespace ui
