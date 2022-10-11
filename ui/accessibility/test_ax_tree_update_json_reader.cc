// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/test_ax_tree_update_json_reader.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/string_split.h"
#include "ui/accessibility/ax_enum_util.h"

namespace {

using RoleConversions = const std::map<std::string, ax::mojom::Role>;

// The 3 lists below include all terms that are not parsed now if they are in a
// JSON file. Since this class is only used for testing, we will only encounter
// errors regarding that in the following two cases:
// - We add a new JSON file (or modify one) for testing that includes different
//   unsupported.
// - There is a new property added to AXNode that is not covered in the existing
//   JSON parsor and a test relies on it.
// In both above cases, existing tests will catch the issue and warn about the
// missing/changed property.
const base::flat_set<std::string> kUnusedAxNodeProperties = {
    "controls", "describedby", "details",    "disabled", "editable",
    "focused",  "hidden",      "hiddenRoot", "live",     "multiline",
    "readonly", "relevant",    "required",   "settable"};

const base::flat_set<std::string> kUnusedAxNodeItems = {
    "frameId", "ignoredReasons", "parentId"};

const base::flat_set<std::string> kUnusedStyles = {
    "background-image", "background-size", "clip",         "font-style",
    "margin-bottom",    "margin-left",     "margin-right", "margin-top",
    "opacity",          "padding-bottom",  "padding-left", "padding-right",
    "padding-top",      "position",        "text-align",   "text-decoration",
    "z-index"};

int GetAsInt(const base::Value& value) {
  if (value.is_int())
    return value.GetInt();
  if (value.is_string())
    return atoi(value.GetString().c_str());

  NOTREACHED() << "Unexpected: " << value;
  return 0;
}

double GetAsDouble(const base::Value& value) {
  if (value.is_double())
    return value.GetDouble();
  if (value.is_int())
    return value.GetInt();
  if (value.is_string())
    return atof(value.GetString().c_str());

  NOTREACHED() << "Unexpected: " << value;
  return 0;
}

bool GetAsBoolean(const base::Value& value) {
  if (value.is_bool())
    return value.GetBool();
  if (value.is_string()) {
    if (value.GetString() == "false")
      return false;
    if (value.GetString() == "true")
      return true;
  }

  NOTREACHED() << "Unexpected: " << value;
  return false;
}

void GetTypeAndValue(const base::Value& node,
                     std::string& type,
                     std::string& value) {
  type = node.GetDict().Find("type")->GetString();
  value = node.GetDict().Find("value")->GetString();
}

ui::AXNodeID AddNode(ui::AXTreeUpdate& tree_update,
                     const base::Value& node,
                     RoleConversions* role_conversions);

void ParseAxNodeChildIds(ui::AXNodeData& node_data,
                         const base::Value& child_ids) {
  for (const auto& item : child_ids.GetList())
    node_data.child_ids.push_back(GetAsInt(item));
}

void ParseAxNodeDescription(ui::AXNodeData& node_data,
                            const base::Value& description) {
  std::string type, value;
  GetTypeAndValue(description, type, value);
  DCHECK_EQ(type, "computedString");
  node_data.SetDescription(value);
}

void ParseAxNodeName(ui::AXNodeData& node_data, const base::Value& name) {
  std::string type, value;
  GetTypeAndValue(name, type, value);
  DCHECK_EQ(type, "computedString");
  node_data.SetName(value);
}

void ParseAxNodeProperties(ui::AXNodeData& node_data,
                           const base::Value& properties) {
  if (properties.is_list()) {
    for (const auto& item : properties.GetList())
      ParseAxNodeProperties(node_data, item);
    return;
  }

  const std::string prop_type = properties.GetDict().Find("name")->GetString();
  const base::Value* prop_value =
      properties.GetDict().Find("value")->GetDict().Find("value");

  if (prop_type == "atomic") {
    node_data.AddBoolAttribute(
        ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot,
        !GetAsBoolean(*prop_value));
  } else if (prop_type == "focusable") {
    if (GetAsBoolean(*prop_value))
      node_data.AddState(ax::mojom::State::kFocusable);
  } else if (prop_type == "expanded") {
    if (GetAsBoolean(*prop_value))
      node_data.AddState(ax::mojom::State::kExpanded);
  } else if (prop_type == "hasPopup") {
    node_data.SetHasPopup(
        ui::ParseAXEnum<ax::mojom::HasPopup>(prop_value->GetString().c_str()));
  } else if (prop_type == "invalid") {
    node_data.SetInvalidState(GetAsBoolean(*prop_value)
                                  ? ax::mojom::InvalidState::kTrue
                                  : ax::mojom::InvalidState::kFalse);
  } else if (prop_type == "level") {
    node_data.AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                              GetAsInt(*prop_value));
  } else {
    DCHECK(base::Contains(kUnusedAxNodeProperties, prop_type)) << prop_type;
  }
}

ax::mojom::Role RoleFromString(std::string role,
                               RoleConversions* role_conversions) {
  const auto& item = role_conversions->find(role);
  DCHECK(item != role_conversions->end()) << role;
  return item->second;
}

void ParseAxNodeRole(ui::AXNodeData& node_data,
                     const base::Value& role,
                     RoleConversions* role_conversions) {
  const std::string role_type = role.GetDict().Find("type")->GetString();
  std::string role_value = role.GetDict().Find("value")->GetString();

  DCHECK(role_type == "role" || role_type == "internalRole");

  node_data.role = RoleFromString(role_value, role_conversions);
}

void ParseAxNode(ui::AXNodeData& node_data,
                 const base::Value& ax_node,
                 RoleConversions* role_conversions) {
  // Store the name and set it at the end because |AXNodeData::SetName|
  // expects a valid role to have already been set prior to calling it.
  base::Value name_value;
  for (const auto item : ax_node.GetDict()) {
    if (item.first == "backendDOMNodeId") {
      node_data.AddIntAttribute(ax::mojom::IntAttribute::kDOMNodeId,
                                GetAsInt(item.second));
    } else if (item.first == "childIds") {
      ParseAxNodeChildIds(node_data, item.second);
    } else if (item.first == "description") {
      ParseAxNodeDescription(node_data, item.second);
    } else if (item.first == "ignored") {
      DCHECK(item.second.is_bool());
      if (item.second.GetBool())
        node_data.AddState(ax::mojom::State::kIgnored);
    } else if (item.first == "name") {
      name_value = item.second.Clone();
    } else if (item.first == "nodeId") {
      node_data.id = GetAsInt(item.second);
    } else if (item.first == "properties") {
      ParseAxNodeProperties(node_data, item.second);
    } else if (item.first == "role") {
      ParseAxNodeRole(node_data, item.second, role_conversions);
    } else {
      DCHECK(base::Contains(kUnusedAxNodeItems, item.first)) << item.first;
    }
  }
  if (!name_value.is_none())
    ParseAxNodeName(node_data, name_value);
}

void ParseChildren(ui::AXTreeUpdate& tree_update,
                   const base::Value& children,
                   RoleConversions* role_conversions) {
  for (const auto& child : children.GetList())
    AddNode(tree_update, child, role_conversions);
}

// Converts "rgb(R,G,B)" or "rgba(R,G,B,A)" to one ARGB integer where R,G, and B
// are integers and A is float < 1.
uint32_t ConvertRgbaStringToArgbInt(const std::string& argb_string) {
  std::vector<std::string> values = base::SplitString(
      argb_string, ",()", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  uint32_t a, r, g, b;

  if (values.size() == 4 && values[0] == "rgb") {
    a = 0;
  } else if (values.size() == 5 && values[0] == "rgba") {
    a = base::ClampRound(atof(values[4].c_str()) * 255);
  } else {
    NOTREACHED() << "Unexpected color value: " << argb_string;
    return -1;
  }

  r = atoi(values[1].c_str());
  g = atoi(values[2].c_str());
  b = atoi(values[3].c_str());

  return (a << 24) + (r << 16) + (g << 8) + b;
}

void ParseStyle(ui::AXNodeData& node_data, const base::Value& style) {
  const std::string& name = style.GetDict().Find("name")->GetString();
  const std::string& value = style.GetDict().Find("value")->GetString();

  if (name == "color") {
    node_data.AddIntAttribute(ax::mojom::IntAttribute::kColor,
                              ConvertRgbaStringToArgbInt(value));
  } else if (name == "direction") {
    node_data.AddIntAttribute(
        ax::mojom::IntAttribute::kTextDirection,
        static_cast<int>(
            ui::ParseAXEnum<ax::mojom::WritingDirection>(value.c_str())));
  } else if (name == "display") {
    node_data.AddStringAttribute(ax::mojom::StringAttribute::kDisplay, value);
  } else if (name == "font-size") {
    // Drop the 'px' at the end of font size.
    DCHECK(style.GetDict().Find("value")->is_string());
    node_data.AddFloatAttribute(
        ax::mojom::FloatAttribute::kFontSize,
        atof(value.substr(0, value.length() - 2).c_str()));
  } else if (name == "font-weight") {
    DCHECK(style.GetDict().Find("value")->is_string());
    node_data.AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize,
                                atof(value.c_str()));
  } else if (name == "list-style-type") {
    node_data.AddIntAttribute(
        ax::mojom::IntAttribute::kListStyle,
        static_cast<int>(ui::ParseAXEnum<ax::mojom::ListStyle>(value.c_str())));
  } else if (name == "visibility") {
    if (value == "hidden")
      node_data.AddState(ax::mojom::State::kInvisible);
    else
      DCHECK_EQ(value, "visible");
  } else {
    DCHECK(base::Contains(kUnusedStyles, name)) << name;
  }
}

void ParseExtras(ui::AXNodeData& node_data, const base::Value& extras) {
  for (const auto extra : extras.GetDict()) {
    const base::Value::List& items = extra.second.GetList();
    if (extra.first == "bounds") {
      node_data.relative_bounds.bounds.set_x(GetAsDouble(items[0]));
      node_data.relative_bounds.bounds.set_y(GetAsDouble(items[1]));
      node_data.relative_bounds.bounds.set_width(GetAsDouble(items[2]));
      node_data.relative_bounds.bounds.set_height(GetAsDouble(items[3]));
    } else if (extra.first == "styles") {
      for (const auto& style : items)
        ParseStyle(node_data, style);
    } else {
      NOTREACHED() << "Unexpected: " << extra.first;
    }
  }
}

// Adds a node and returns its id.
ui::AXNodeID AddNode(ui::AXTreeUpdate& tree_update,
                     const base::Value& node,
                     RoleConversions* role_conversions) {
  ui::AXNodeData node_data;

  // Store the string and set it at the end because |AXNodeData::SetName|
  // expects a valid role to have already been set prior to calling it.
  std::string name_string;

  for (const auto item : node.GetDict()) {
    if (item.first == "axNode") {
      ParseAxNode(node_data, item.second, role_conversions);
    } else if (item.first == "backendDomId") {
      node_data.AddIntAttribute(ax::mojom::IntAttribute::kDOMNodeId,
                                GetAsInt(item.second));
    } else if (item.first == "children") {
      ParseChildren(tree_update, item.second, role_conversions);
    } else if (item.first == "description") {
      node_data.SetDescription(item.second.GetString());
    } else if (item.first == "extras") {
      ParseExtras(node_data, item.second);
    } else if (item.first == "interesting") {
      // Not used yet, boolean.
    } else if (item.first == "name") {
      name_string = item.second.GetString();
    } else if (item.first == "role") {
      node_data.role =
          RoleFromString(item.second.GetString(), role_conversions);
    } else {
      NOTREACHED() << "Unexpected: " << item.first;
    }
  }

  node_data.SetName(name_string);

  tree_update.nodes.push_back(node_data);

  return node_data.id;
}

}  // namespace

namespace ui {

AXTreeUpdate AXTreeUpdateFromJSON(const base::Value& json,
                                  RoleConversions* role_conversions) {
  AXTreeUpdate tree_update;

  // Input should be a list with one item, which is the root node.
  DCHECK(json.is_list() && json.GetList().size() == 1);

  tree_update.root_id =
      AddNode(tree_update, json.GetList().front(), role_conversions);

  // |AddNode| adds child nodes before parent nodes, while AXTree deserializer
  // expects parents first.
  std::reverse(tree_update.nodes.begin(), tree_update.nodes.end());

  return tree_update;
}

}  // namespace ui
