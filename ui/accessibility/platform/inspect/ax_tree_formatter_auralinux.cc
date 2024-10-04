// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/accessibility/platform/inspect/ax_tree_formatter_auralinux.h"

#include <dbus/dbus.h>

#include <utility>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"
#include "ui/accessibility/platform/inspect/ax_call_statement_invoker_auralinux.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_auralinux.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"
#include "ui/accessibility/platform/inspect/ax_script_instruction.h"

namespace ui {

// Used in dictionary to disambiguate property vs object attribute when they
// have the same name, e.g. "description".
// In the final output, they will show up differently:
// description='xxx' (property)
// description:xxx (object attribute)
static constexpr char kObjectAttributePrefix[] = "@";

AXTreeFormatterAuraLinux::AXTreeFormatterAuraLinux() = default;

AXTreeFormatterAuraLinux::~AXTreeFormatterAuraLinux() {}

base::Value::Dict AXTreeFormatterAuraLinux::BuildTreeForSelector(
    const AXTreeSelector& selector) const {
  AtspiAccessible* node = FindAccessible(selector);
  if (!node) {
    return base::Value::Dict();
  }

  base::Value::Dict dict;
  RecursiveBuildTree(node, &dict);
  return dict;
}

std::string AXTreeFormatterAuraLinux::EvaluateScript(
    const AXTreeSelector& selector,
    const AXInspectScenario& scenario) const {
  AtspiAccessible* platform_root = FindAccessible(selector);
  if (!platform_root) {
    return "error no accessibility tree found";
  }

  const std::vector<AXScriptInstruction>& instructions =
      scenario.script_instructions;
  size_t end_index = instructions.size();

  std::vector<std::string> scripts;
  AXTreeIndexerAuraLinux indexer(platform_root);
  std::map<std::string, Target> storage;
  AXCallStatementInvokerAuraLinux invoker(&indexer, &storage);
  for (size_t index = 0; index < end_index; index++) {
    if (instructions[index].IsComment()) {
      scripts.emplace_back(instructions[index].AsComment());
      continue;
    }

    DCHECK(instructions[index].IsScript());
    const AXPropertyNode& property_node = instructions[index].AsScript();

    AXOptionalObject value = invoker.Invoke(property_node);
    if (value.IsUnsupported()) {
      continue;
    }

    scripts.emplace_back(property_node.ToString() + "=" +
                         AXCallStatementInvokerAuraLinux::ToString(value));
  }

  std::string contents;
  for (const std::string& script : scripts) {
    std::string line;
    WriteAttribute(true, script, &line);
    contents += line + "\n";
  }
  return contents;
}

AtkObject* GetAtkObject(AXPlatformNodeDelegate* node) {
  DCHECK(node);

  AtkObject* atk_node = node->GetNativeViewAccessible();
  DCHECK(atk_node);

  return atk_node;
}

base::Value::Dict AXTreeFormatterAuraLinux::BuildTree(
    AXPlatformNodeDelegate* root) const {
  base::Value::Dict dict;
  RecursiveBuildTree(GetAtkObject(root), &dict);
  return dict;
}

base::Value::Dict AXTreeFormatterAuraLinux::BuildNode(
    AXPlatformNodeDelegate* node) const {
  base::Value::Dict dict;
  AddProperties(GetAtkObject(node), &dict);
  return dict;
}

void AXTreeFormatterAuraLinux::RecursiveBuildTree(
    AtkObject* atk_node,
    base::Value::Dict* dict) const {
  AXPlatformNodeAuraLinux* platform_node =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_node);
  DCHECK(platform_node);

  AXPlatformNodeDelegate* node = platform_node->GetDelegate();
  DCHECK(node);

  if (!ShouldDumpNode(*node))
    return;

  AddProperties(atk_node, dict);
  if (!ShouldDumpChildren(*node))
    return;

  auto child_count = atk_object_get_n_accessible_children(atk_node);
  if (child_count <= 0)
    return;

  base::Value::List children;
  for (auto i = 0; i < child_count; i++) {
    base::Value::Dict child_dict;

    AtkObject* atk_child = atk_object_ref_accessible_child(atk_node, i);
    CHECK(atk_child);

    RecursiveBuildTree(atk_child, &child_dict);
    g_object_unref(atk_child);

    children.Append(std::move(child_dict));
  }

  dict->Set(kChildrenDictAttr, std::move(children));
}

void AXTreeFormatterAuraLinux::RecursiveBuildTree(
    AtspiAccessible* node,
    base::Value::Dict* dict) const {
  AddProperties(node, dict);

  GError* error = nullptr;
  int child_count = atspi_accessible_get_child_count(node, &error);
  if (error) {
    g_clear_error(&error);
    return;
  }

  if (child_count <= 0)
    return;

  base::Value::List children;
  for (int i = 0; i < child_count; i++) {
    base::Value::Dict child_dict;

    AtspiAccessible* child =
        atspi_accessible_get_child_at_index(node, i, &error);
    if (error) {
      child_dict.Set("error", "[Error retrieving child]");
      g_clear_error(&error);
      continue;
    }

    CHECK(child);
    RecursiveBuildTree(child, &child_dict);
    children.Append(std::move(child_dict));
  }

  dict->Set(kChildrenDictAttr, std::move(children));
}

void AXTreeFormatterAuraLinux::AddHypertextProperties(
    AtkObject* atk_object,
    base::Value::Dict* dict) const {
  if (!ATK_IS_TEXT(atk_object) || !ATK_IS_HYPERTEXT(atk_object))
    return;

  AtkText* atk_text = ATK_TEXT(atk_object);
  gchar* character_text = atk_text_get_text(atk_text, 0, -1);
  if (!character_text)
    return;

  base::Value::List values;

  // Each link in the atk_text is represented by the multibyte unicode character
  // U+FFFC, which in UTF-8 is 0xEF 0xBF 0xBC. We will replace each instance of
  // this character with something slightly more useful.

  std::string text(character_text);
  AtkHypertext* hypertext = ATK_HYPERTEXT(atk_object);
  int link_count = atk_hypertext_get_n_links(hypertext);
  if (link_count > 0) {
    for (int link_index = link_count - 1; link_index >= 0; link_index--) {
      // Replacement text
      std::string link_str("<obj");
      if (link_index >= 0) {
        base::StringAppendF(&link_str, "%d>", link_index);
      } else {
        base::StringAppendF(&link_str, ">");
      }

      AtkHyperlink* link = atk_hypertext_get_link(hypertext, link_index);
      if (!link)
        continue;  // TODO(aleventhal) Change to DCHECK(link);

#if DCHECK_IS_ON()
      AtkObject* link_obj = atk_hyperlink_get_object(link, 0);
      DCHECK(link_obj);
      int index_in_parent = atk_object_get_index_in_parent(link_obj);
      DCHECK_GE(index_in_parent, 0);
#endif

      int utf8_offset = atk_hyperlink_get_start_index(link);
      gchar* link_start = g_utf8_offset_to_pointer(character_text, utf8_offset);
      int offset = link_start - character_text;

      std::string replacement_char = "\uFFFC";
      base::ReplaceFirstSubstringAfterOffset(&text, offset, replacement_char,
                                             link_str);
    }
  }

  values.Append(base::StringPrintf("hypertext='%s'", text.c_str()));
  dict->Set("hypertext", std::move(values));

  g_free(character_text);
}

void AXTreeFormatterAuraLinux::AddTextProperties(
    AtkObject* atk_object,
    base::Value::Dict* dict) const {
  if (!ATK_IS_TEXT(atk_object))
    return;

  AtkText* atk_text = ATK_TEXT(atk_object);

  base::Value::List text_values;
  int character_count = atk_text_get_character_count(atk_text);
  text_values.Append(base::StringPrintf("character_count=%i", character_count));

  int caret_offset = atk_text_get_caret_offset(atk_text);
  if (caret_offset != -1)
    text_values.Append(base::StringPrintf("caret_offset=%i", caret_offset));

  int selection_start, selection_end;
  char* selection_text =
      atk_text_get_selection(atk_text, 0, &selection_start, &selection_end);
  if (selection_text) {
    g_free(selection_text);
    text_values.Append(
        base::StringPrintf("selection_start=%i", selection_start));
    text_values.Append(base::StringPrintf("selection_end=%i", selection_end));
  }

  auto add_attribute_set_values = [](gpointer value, gpointer list) {
    const AtkAttribute* attribute = static_cast<const AtkAttribute*>(value);
    static_cast<base::Value::List*>(list)->Append(
        base::StringPrintf("%s=%s", attribute->name, attribute->value));
  };

  int current_offset = 0, start_offset, end_offset;
  while (current_offset < character_count) {
    AtkAttributeSet* text_attributes = atk_text_get_run_attributes(
        atk_text, current_offset, &start_offset, &end_offset);
    text_values.Append(base::StringPrintf("offset=%i", start_offset));
    g_slist_foreach(text_attributes, add_attribute_set_values, &text_values);
    atk_attribute_set_free(text_attributes);

    current_offset = end_offset;
  }

  gchar* character_text = atk_text_get_text(atk_text, 0, -1);
  if (character_text) {
    std::string text(character_text);
    text_values.Append(base::StringPrintf("text='%s'", text.c_str()));
    g_free(character_text);
  }

  dict->Set("text", std::move(text_values));
}

void AXTreeFormatterAuraLinux::AddActionProperties(
    AtkObject* atk_object,
    base::Value::Dict* dict) const {
  if (!ATK_IS_ACTION(atk_object))
    return;

  AtkAction* action = ATK_ACTION(atk_object);
  int action_count = atk_action_get_n_actions(action);
  if (!action_count)
    return;

  base::Value::List actions;
  for (int i = 0; i < action_count; i++) {
    const char* name = atk_action_get_name(action, i);
    actions.Append(name ? name : "");
  }
  dict->Set("actions", std::move(actions));
}

void AXTreeFormatterAuraLinux::AddRelationProperties(
    AtkObject* atk_object,
    base::Value::Dict* dict) const {
  AtkRelationSet* relation_set = atk_object_ref_relation_set(atk_object);
  base::Value::List relations;

  for (int i = ATK_RELATION_NULL; i < ATK_RELATION_LAST_DEFINED; i++) {
    AtkRelationType relation_type = static_cast<AtkRelationType>(i);
    if (atk_relation_set_contains(relation_set, relation_type)) {
      AtkRelation* relation =
          atk_relation_set_get_relation_by_type(relation_set, relation_type);
      DCHECK(relation);

      relations.Append(ToString(relation));
    }
  }

  g_object_unref(relation_set);
  dict->Set("relations", std::move(relations));
}

std::string AXTreeFormatterAuraLinux::ToString(AtkRelation* relation) {
  std::string relation_name =
      atk_relation_type_get_name(relation->relationship);
  GPtrArray* relation_targets = atk_relation_get_target(relation);
  DCHECK(relation_targets);

  std::vector<std::string> target_roles(relation_targets->len);
  for (guint i = 0; i < relation_targets->len; i++) {
    AtkObject* atk_target =
        static_cast<AtkObject*>(g_ptr_array_index(relation_targets, i));
    DCHECK(atk_target);
    target_roles[i] = atk_role_get_name(atk_object_get_role(atk_target));
  }

  // We need to alphabetically sort the roles so tests don't flake from the
  // order of `relation_targets`.
  std::sort(target_roles.begin(), target_roles.end());
  return base::StrCat(
      {relation_name, "=[", base::JoinString(target_roles, ","), "]"});
}

void AXTreeFormatterAuraLinux::AddValueProperties(
    AtkObject* atk_object,
    base::Value::Dict* dict) const {
  if (!ATK_IS_VALUE(atk_object))
    return;

  base::Value::List value_properties;
  AtkValue* value = ATK_VALUE(atk_object);
  GValue current = G_VALUE_INIT;
  g_value_init(&current, G_TYPE_FLOAT);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  atk_value_get_current_value(value, &current);

  value_properties.Append(
      base::StringPrintf("current=%f", g_value_get_float(&current)));

  GValue minimum = G_VALUE_INIT;
  g_value_init(&minimum, G_TYPE_FLOAT);
  atk_value_get_minimum_value(value, &minimum);
  value_properties.Append(
      base::StringPrintf("minimum=%f", g_value_get_float(&minimum)));

  GValue maximum = G_VALUE_INIT;
  g_value_init(&maximum, G_TYPE_FLOAT);
  atk_value_get_maximum_value(value, &maximum);
#pragma clang diagnostic pop

  value_properties.Append(
      base::StringPrintf("maximum=%f", g_value_get_float(&maximum)));
  dict->Set("value", std::move(value_properties));
}

void AXTreeFormatterAuraLinux::AddTableProperties(
    AtkObject* atk_object,
    base::Value::Dict* dict) const {
  if (!ATK_IS_TABLE(atk_object))
    return;

  // Column details.
  AtkTable* table = ATK_TABLE(atk_object);
  int n_cols = atk_table_get_n_columns(table);
  base::Value::List table_properties;
  table_properties.Append(base::StringPrintf("cols=%i", n_cols));

  std::vector<std::string> col_headers;
  for (int i = 0; i < n_cols; i++) {
    std::string header = atk_table_get_column_description(table, i);
    if (!header.empty())
      col_headers.push_back(base::StringPrintf("'%s'", header.c_str()));
  }

  if (!col_headers.size())
    col_headers.push_back("NONE");

  table_properties.Append(base::StringPrintf(
      "headers=(%s);", base::JoinString(col_headers, ", ").c_str()));

  // Row details.
  int n_rows = atk_table_get_n_rows(table);
  table_properties.Append(base::StringPrintf("rows=%i", n_rows));

  std::vector<std::string> row_headers;
  for (int i = 0; i < n_rows; i++) {
    std::string header = atk_table_get_row_description(table, i);
    if (!header.empty())
      row_headers.push_back(base::StringPrintf("'%s'", header.c_str()));
  }

  if (!row_headers.size())
    row_headers.push_back("NONE");

  table_properties.Append(base::StringPrintf(
      "headers=(%s);", base::JoinString(row_headers, ", ").c_str()));

  // Caption details.
  AtkObject* caption = atk_table_get_caption(table);
  table_properties.Append(
      base::StringPrintf("caption=%s;", caption ? "true" : "false"));

  // Summarize information about the cells from the table's perspective here.
  std::vector<std::string> span_info;
  for (int r = 0; r < n_rows; r++) {
    for (int c = 0; c < n_cols; c++) {
      int row_span = atk_table_get_row_extent_at(table, r, c);
      int col_span = atk_table_get_column_extent_at(table, r, c);
      if (row_span != 1 || col_span != 1) {
        span_info.push_back(base::StringPrintf("cell at %i,%i: %ix%i", r, c,
                                               row_span, col_span));
      }
    }
  }
  if (!span_info.size())
    span_info.push_back("all: 1x1");

  table_properties.Append(base::StringPrintf(
      "spans=(%s)", base::JoinString(span_info, ", ").c_str()));
  dict->Set("table", std::move(table_properties));
}

void AXTreeFormatterAuraLinux::AddTableCellProperties(
    const AXPlatformNodeAuraLinux* node,
    AtkObject* atk_object,
    base::Value::Dict* dict) const {
  AtkRole role = atk_object_get_role(atk_object);
  if (role != ATK_ROLE_TABLE_CELL && role != ATK_ROLE_COLUMN_HEADER &&
      role != ATK_ROLE_ROW_HEADER) {
    return;
  }

  int row = 0, col = 0, row_span = 0, col_span = 0;
  int n_row_headers = 0, n_column_headers = 0;

  AtkTableCell* cell = G_TYPE_CHECK_INSTANCE_CAST(
      (atk_object), atk_table_cell_get_type(), AtkTableCell);

  atk_table_cell_get_row_column_span(cell, &row, &col, &row_span, &col_span);

  GPtrArray* column_headers = atk_table_cell_get_column_header_cells(cell);
  n_column_headers = column_headers->len;
  g_ptr_array_unref(column_headers);

  GPtrArray* row_headers = atk_table_cell_get_row_header_cells(cell);
  n_row_headers = row_headers->len;
  g_ptr_array_unref(row_headers);

  std::vector<std::string> cell_info;
  cell_info.push_back(base::StringPrintf("row=%i", row));
  cell_info.push_back(base::StringPrintf("col=%i", col));
  cell_info.push_back(base::StringPrintf("row_span=%i", row_span));
  cell_info.push_back(base::StringPrintf("col_span=%i", col_span));
  cell_info.push_back(base::StringPrintf("n_row_headers=%i", n_row_headers));
  cell_info.push_back(base::StringPrintf("n_col_headers=%i", n_column_headers));

  base::Value::List cell_properties;
  cell_properties.Append(
      base::StringPrintf("(%s)", base::JoinString(cell_info, ", ").c_str()));
  dict->Set("cell", std::move(cell_properties));
}

void AXTreeFormatterAuraLinux::AddProperties(AtkObject* atk_object,
                                             base::Value::Dict* dict) const {
  AXPlatformNodeAuraLinux* platform_node =
      AXPlatformNodeAuraLinux::FromAtkObject(atk_object);
  DCHECK(platform_node);

  AXPlatformNodeDelegate* node = platform_node->GetDelegate();
  DCHECK(node);

  dict->Set("id", node->GetId());

  AtkRole role = atk_object_get_role(atk_object);
  if (role != ATK_ROLE_UNKNOWN) {
    dict->Set("role", AtkRoleToString(role));
  }

  const gchar* name = atk_object_get_name(atk_object);
  if (name)
    dict->Set("name", std::string(name));
  const gchar* description = atk_object_get_description(atk_object);
  if (description)
    dict->Set("description", std::string(description));

  AtkStateSet* state_set = atk_object_ref_state_set(atk_object);
  base::Value::List states;
  for (int i = ATK_STATE_INVALID; i < ATK_STATE_LAST_DEFINED; i++) {
    AtkStateType state_type = static_cast<AtkStateType>(i);
    if (atk_state_set_contains_state(state_set, state_type))
      states.Append(atk_state_type_get_name(state_type));
  }
  dict->Set("states", std::move(states));
  g_object_unref(state_set);

  AtkAttributeSet* attributes = atk_object_get_attributes(atk_object);
  for (AtkAttributeSet* attr = attributes; attr; attr = attr->next) {
    AtkAttribute* attribute = static_cast<AtkAttribute*>(attr->data);
    dict->SetByDottedPath(std::string(kObjectAttributePrefix) + attribute->name,
                          attribute->value);
  }
  atk_attribute_set_free(attributes);

  AddTextProperties(atk_object, dict);
  AddHypertextProperties(atk_object, dict);
  AddActionProperties(atk_object, dict);
  AddRelationProperties(atk_object, dict);
  AddValueProperties(atk_object, dict);
  AddTableProperties(atk_object, dict);
  AddTableCellProperties(platform_node, atk_object, dict);
}

void AXTreeFormatterAuraLinux::AddProperties(AtspiAccessible* node,
                                             base::Value::Dict* dict) const {
  GError* error = nullptr;
  char* role_name = atspi_accessible_get_role_name(node, &error);
  if (!error)
    dict->Set("role", role_name);
  g_clear_error(&error);
  free(role_name);

  char* name = atspi_accessible_get_name(node, &error);
  if (!error)
    dict->Set("name", name);
  g_clear_error(&error);
  free(name);

  error = nullptr;
  char* description = atspi_accessible_get_description(node, &error);
  if (!error)
    dict->Set("description", description);
  g_clear_error(&error);
  free(description);

  error = nullptr;
  GHashTable* attributes = atspi_accessible_get_attributes(node, &error);
  if (!error) {
    GHashTableIter i;
    void* key = nullptr;
    void* value = nullptr;

    g_hash_table_iter_init(&i, attributes);
    while (g_hash_table_iter_next(&i, &key, &value)) {
      dict->SetByDottedPath(static_cast<char*>(key), static_cast<char*>(value));
    }
  }
  g_clear_error(&error);
  g_hash_table_unref(attributes);

  AtspiStateSet* atspi_states = atspi_accessible_get_state_set(node);
  GArray* state_array = atspi_state_set_get_states(atspi_states);
  base::Value::List states;
  for (unsigned i = 0; i < state_array->len; i++) {
    AtspiStateType state_type = g_array_index(state_array, AtspiStateType, i);
    states.Append(ATSPIStateToString(state_type));
  }
  dict->Set("states", std::move(states));
  g_array_free(state_array, TRUE);
  g_object_unref(atspi_states);
}

const char* const ATK_OBJECT_ATTRIBUTES[] = {
    "atomic",
    "autocomplete",
    "braillelabel",
    "brailleroledescription",
    "busy",
    "checkable",
    "class",
    "colcount",
    "colindex",
    "colspan",
    "coltext",
    "colindextext",
    "container-atomic",
    "container-busy",
    "container-live",
    "container-relevant",
    "current",
    "description",
    "description-from",
    "details-from",
    "details-roles",
    "display",
    "dropeffect",
    "explicit-name",
    "grabbed",
    "haspopup",
    "hidden",
    "id",
    "keyshortcuts",
    "level",
    "link-target",
    "live",
    "name-from",
    "placeholder",
    "posinset",
    "relevant",
    "roledescription",
    "rowcount",
    "rowindex",
    "rowindextext",
    "rowspan",
    "rowtext",
    "setsize",
    "sort",
    "src",
    "table-cell-index",
    "tag",
    "text-align",
    "text-indent",
    "text-input-type",
    "text-position",
    "valuemin",
    "valuemax",
    "valuenow",
    "valuetext",
    "xml-roles",
};

std::string AXTreeFormatterAuraLinux::ProcessTreeForOutput(
    const base::Value::Dict& node) const {
  const std::string* error_value = node.FindString("error");
  if (error_value)
    return *error_value;

  std::string line;
  const std::string* role_value = node.FindString("role");
  if (role_value && !role_value->empty()) {
    WriteAttribute(true, base::StringPrintf("[%s]", role_value->c_str()),
                   &line);
  }

  const std::string* name_value = node.FindString("name");
  if (name_value) {
    WriteAttribute(true, base::StringPrintf("name='%s'", name_value->c_str()),
                   &line);
  }

  const std::string* description_value = node.FindString("description");
  if (description_value) {
    WriteAttribute(
        false,
        base::StringPrintf("description='%s'", description_value->c_str()),
        &line);
  }

  const base::Value::List* states_value = node.FindList("states");
  if (states_value) {
    for (const auto& entry : *states_value) {
      const std::string* state_value = entry.GetIfString();
      if (state_value)
        WriteAttribute(false, *state_value, &line);
    }
  }

  const base::Value::List* action_names_list = node.FindList("actions");
  if (action_names_list) {
    std::vector<std::string> action_names;
    for (const auto& entry : *action_names_list) {
      const std::string* action_name = entry.GetIfString();
      if (action_name)
        action_names.push_back(*action_name);
    }
    std::string actions_str = base::JoinString(action_names, ", ");
    if (actions_str.size()) {
      WriteAttribute(false,
                     base::StringPrintf("actions=(%s)", actions_str.c_str()),
                     &line);
    }
  }

  const base::Value::List* relations_value = node.FindList("relations");
  if (relations_value) {
    for (const auto& entry : *relations_value) {
      const std::string* relation_value = entry.GetIfString();
      if (relation_value) {
        // By default, exclude embedded-by because that should appear on every
        // top-level document object. The other relation types are less common
        // and thus almost always of interest when testing.
        WriteAttribute(!relation_value->starts_with("embedded-by"),
                       *relation_value, &line);
      }
    }
  }

  for (const char* attribute_name : ATK_OBJECT_ATTRIBUTES) {
    const std::string* attribute_value =
        node.FindString(std::string(kObjectAttributePrefix) + attribute_name);
    // ATK object attributes are stored with a prefix, in order to disambiguate
    // from other properties with the same name (e.g. description).
    if (attribute_value) {
      WriteAttribute(
          false,
          base::StringPrintf("%s:%s", attribute_name, attribute_value->c_str()),
          &line);
    }
  }

  const base::Value::List* value_info = node.FindList("value");
  if (value_info) {
    for (const auto& entry : *value_info) {
      const std::string* value_property = entry.GetIfString();
      if (value_property)
        WriteAttribute(true, *value_property, &line);
    }
  }

  const base::Value::List* table_info = node.FindList("table");
  if (table_info) {
    for (const auto& entry : *table_info) {
      const std::string* table_property = entry.GetIfString();
      if (table_property)
        WriteAttribute(true, *table_property, &line);
    }
  }

  const base::Value::List* cell_info = node.FindList("cell");
  if (cell_info) {
    for (const auto& entry : *cell_info) {
      const std::string* cell_property = entry.GetIfString();
      if (cell_property)
        WriteAttribute(true, *cell_property, &line);
    }
  }

  const base::Value::List* text_info = node.FindList("text");
  if (text_info) {
    for (const auto& entry : *text_info) {
      const std::string* text_property = entry.GetIfString();
      if (text_property)
        WriteAttribute(false, *text_property, &line);
    }
  }

  const base::Value::List* hypertext_info = node.FindList("hypertext");
  if (hypertext_info) {
    for (const auto& entry : *hypertext_info) {
      const std::string* hypertext_property = entry.GetIfString();
      if (hypertext_property)
        WriteAttribute(false, *hypertext_property, &line);
    }
  }

  return line;
}

}  // namespace ui
