// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/test_ax_tree_update_json_reader.h"

#include "ui/accessibility/ax_enum_util.h"

namespace {

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

ui::AXNodeID AddNode(ui::AXTreeUpdate& tree_update, const base::Value& node);

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
  } else if (prop_type == "controls") {
    // TODO(https://crbug.com/1278249): Add, string value.
  } else if (prop_type == "focusable") {
    if (GetAsBoolean(*prop_value))
      node_data.AddState(ax::mojom::State::kFocusable);
  } else if (prop_type == "expanded") {
    if (GetAsBoolean(*prop_value))
      node_data.AddState(ax::mojom::State::kExpanded);
  } else if (prop_type == "focused") {
    // TODO(https://crbug.com/1278249): Add, boolean value.
  } else if (prop_type == "hasPopup") {
    node_data.SetHasPopup(
        ui::ParseAXEnum<ax::mojom::HasPopup>(prop_value->GetString().c_str()));
  } else if (prop_type == "hidden") {
    // Boolean, not used.
  } else if (prop_type == "hiddenRoot") {
    // Null, not used.
  } else if (prop_type == "invalid") {
    node_data.SetInvalidState(GetAsBoolean(*prop_value)
                                  ? ax::mojom::InvalidState::kTrue
                                  : ax::mojom::InvalidState::kFalse);
  } else if (prop_type == "level") {
    node_data.AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                              GetAsInt(*prop_value));
  } else if (prop_type == "live") {
    // TODO(https://crbug.com/1278249): Add, string, e.g. "polite".
  } else if (prop_type == "relevant") {
    // TODO(https://crbug.com/1278249): Add, string, e.g. "additions text"
  } else {
    NOTREACHED() << "Unexpected: " << prop_type;
  }
}

// In a few cases, role values in JSON file are different from the ones used in
// AxEnumUtils::ToString and need preprocessing. This function and the entire
// class will be removed after Screen2X starts using Chrome for training data
// proto generation.
ax::mojom::Role RoleFromString(std::string role) {
  if (role == "combobox")
    role = "comboBoxGrouping";
  else if (role == "contentinfo")
    role = "contentInfo";
  else if (role == "DescriptionList")
    role = "descriptionList";
  else if (role == "DescriptionListDetail")
    role = "descriptionListDetail";
  else if (role == "DescriptionListTerm")
    role = "descriptionListTerm";
  else if (role == "generic")
    role = "genericContainer";
  else if (role == "HeaderAsNonLandmark")
    role = "headerAsNonLandmark";
  else if (role == "img")
    role = "image";
  else if (role == "LineBreak")
    role = "lineBreak";
  else if (role == "listitem")
    role = "listItem";
  else if (role == "ListMarker")
    role = "listMarker";
  else if (role == "RootWebArea")
    role = "rootWebArea";
  else if (role == "Section")
    role = "section";
  else if (role == "StaticText")
    role = "staticText";

  return ui::ParseAXEnum<ax::mojom::Role>(role.c_str());
}

void ParseAxNodeRole(ui::AXNodeData& node_data, const base::Value& role) {
  const std::string role_type = role.GetDict().Find("type")->GetString();
  std::string role_value = role.GetDict().Find("value")->GetString();

  DCHECK(role_type == "role" || role_type == "internalRole");

  node_data.role = RoleFromString(role_value);
}

void ParseAxNode(ui::AXNodeData& node_data, const base::Value& ax_node) {
  for (const auto item : ax_node.GetDict()) {
    if (item.first == "backendDOMNodeId") {
      node_data.AddIntAttribute(ax::mojom::IntAttribute::kDOMNodeId,
                                GetAsInt(item.second));
    } else if (item.first == "childIds") {
      ParseAxNodeChildIds(node_data, item.second);
    } else if (item.first == "description") {
      ParseAxNodeDescription(node_data, item.second);
    } else if (item.first == "frameId") {
      // Not used yet, string.
    } else if (item.first == "ignored") {
      DCHECK(item.second.is_bool());
      if (item.second.GetBool())
        node_data.AddState(ax::mojom::State::kIgnored);
    } else if (item.first == "ignoredReasons") {
      // Not used yet, dict with values: ariaHiddenSubtree/Element, ...
    } else if (item.first == "name") {
      ParseAxNodeName(node_data, item.second);
    } else if (item.first == "nodeId") {
      node_data.id = GetAsInt(item.second);
    } else if (item.first == "parentId") {
      // Not used.
    } else if (item.first == "properties") {
      ParseAxNodeProperties(node_data, item.second);
    } else if (item.first == "role") {
      ParseAxNodeRole(node_data, item.second);
    } else {
      NOTREACHED() << "Unexpected: " << item.first;
    }
  }
}

void ParseChildren(ui::AXTreeUpdate& tree_update, const base::Value& children) {
  for (const auto& child : children.GetList())
    AddNode(tree_update, child);
}

// Converts "rgb(R,G,B)" to int(R << 24 + G << 16 + B << 8).
int ConvertRgbStringToRgbaInt(std::string rgb) {
  size_t pos[] = {3, rgb.find(',', pos[0] + 1), rgb.find(',', pos[1] + 1),
                  rgb.length() - 1};

  if (rgb.substr(0, 4) != "rgb(" || pos[1] == std::string::npos ||
      pos[2] == std::string::npos) {
    NOTREACHED() << "Unexpected RGB: " << rgb;
    return -1;
  }

  int rgba = 0;
  for (int i = 0; i < 3; i++) {
    rgba += atoi(rgb.substr(pos[i] + 1, pos[i + 1] - pos[i]).c_str())
            << (24 - i * 8);
  }

  return rgba;
}

void ParseStyle(ui::AXNodeData& node_data, const base::Value& style) {
  const std::string& name = style.GetDict().Find("name")->GetString();
  const std::string& value = style.GetDict().Find("value")->GetString();
  if (name == "background-image") {
    // Not used.
    DCHECK_EQ(value, "none");
  } else if (name == "background-size") {
    // Not used.
    DCHECK_EQ(value, "auto");
  } else if (name == "clip") {
    // Not used, values 'auto', 'rect(...)'.
  } else if (name == "color") {
    node_data.AddIntAttribute(ax::mojom::IntAttribute::kColor,
                              ConvertRgbStringToRgbaInt(value));
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
  } else if (name == "font-style") {
    // Not used.
    DCHECK_EQ(value, "normal");
  } else if (name == "font-weight") {
    DCHECK(style.GetDict().Find("value")->is_string());
    node_data.AddFloatAttribute(ax::mojom::FloatAttribute::kFontSize,
                                atof(value.c_str()));
  } else if (name == "list-style-type") {
    node_data.AddIntAttribute(
        ax::mojom::IntAttribute::kListStyle,
        static_cast<int>(ui::ParseAXEnum<ax::mojom::ListStyle>(value.c_str())));
  } else if (name == "margin-bottom") {
    // Not used.
  } else if (name == "margin-left") {
    // Not used.
  } else if (name == "margin-right") {
    // Not used.
  } else if (name == "margin-top") {
    // Not used.
  } else if (name == "opacity") {
    // Not used.
  } else if (name == "padding-bottom") {
    // Not used.
  } else if (name == "padding-left") {
    // Not used.
  } else if (name == "padding-right") {
    // Not used.
  } else if (name == "padding-top") {
    // Not used.
  } else if (name == "position") {
    // Not used.
    DCHECK(value == "static" || value == "fixed" || value == "relative" ||
           value == "absolute" || value == "sticky")
        << value;
  } else if (name == "text-align") {
    // Not used.
    DCHECK(value == "start" || value == "center" || value == "right" ||
           value == "left")
        << value;
  } else if (name == "text-decoration") {
    // Not used. eg, "underline 1px sold rgb(0,0,0)"
  } else if (name == "visibility") {
    if (value == "invisible")
      node_data.AddState(ax::mojom::State::kInvisible);
    else
      DCHECK_EQ(value, "visible");
  } else if (name == "z-index") {
    // Not used.
    DCHECK(value == "auto" || value == "1" || value == "1000") << value;
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
ui::AXNodeID AddNode(ui::AXTreeUpdate& tree_update, const base::Value& node) {
  ui::AXNodeData node_data;

  for (const auto item : node.GetDict()) {
    if (item.first == "axNode") {
      ParseAxNode(node_data, item.second);
    } else if (item.first == "backendDomId") {
      node_data.AddIntAttribute(ax::mojom::IntAttribute::kDOMNodeId,
                                GetAsInt(item.second));
    } else if (item.first == "children") {
      ParseChildren(tree_update, item.second);
    } else if (item.first == "description") {
      node_data.SetDescription(item.second.GetString());
    } else if (item.first == "extras") {
      ParseExtras(node_data, item.second);
    } else if (item.first == "interesting") {
      // Not used yet, boolean.
    } else if (item.first == "name") {
      node_data.SetName(item.second.GetString());
    } else if (item.first == "role") {
      node_data.role = RoleFromString(item.second.GetString());
    } else {
      NOTREACHED() << "Unexpected: " << item.first;
    }
  }

  tree_update.nodes.push_back(node_data);

  return node_data.id;
}

}  // namespace

namespace ui {

AXTreeUpdate AXTreeUpdateFromJSON(const base::Value& json) {
  AXTreeUpdate tree_update;

  // Input should be a list with one item, which is the root node.
  DCHECK(json.is_list() && json.GetList().size() == 1);

  tree_update.root_id = AddNode(tree_update, json.GetList().front());

  return tree_update;
}

ax::mojom::Role RoleFromStringForTesting(std::string role) {
  return RoleFromString(role);
}

}  // namespace ui
