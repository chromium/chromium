// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_tree_formatter_mac.h"

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ui/accessibility/platform/ax_platform_node_cocoa.h"
#include "ui/accessibility/platform/inspect/ax_element_wrapper_mac.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_mac.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"
#include "ui/accessibility/platform/inspect/ax_script_instruction.h"
#include "ui/accessibility/platform/inspect/ax_transform_mac.h"

// This file uses the deprecated NSObject accessibility interface.
// TODO(crbug.com/41450813): Migrate to the new NSAccessibility interface.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace ui {

namespace {

constexpr char kLocalPositionDictAttr[] = "LocalPosition";

constexpr char kFailedPrefix[] = "_const_ERROR:";
constexpr char kFailedToParseError[] = "_const_ERROR:FAILED_TO_PARSE";
constexpr char kNotApplicable[] = "_const_n/a";

}  // namespace

AXTreeFormatterMac::AXTreeFormatterMac() = default;

AXTreeFormatterMac::~AXTreeFormatterMac() = default;

void AXTreeFormatterMac::AddDefaultFilters(
    std::vector<AXPropertyFilter>* property_filters) {
  static NSArray* default_attributes = @[
    @"AXAutocompleteValue", @"AXDescription", @"AXRole", @"AXSubrole",
    @"AXTitle", @"AXTitleUIElement", @"AXValue"
  ];

  for (NSString* attribute : default_attributes) {
    AddPropertyFilter(property_filters, base::SysNSStringToUTF8(attribute));
  }

  if (show_ids()) {
    AddPropertyFilter(property_filters, "ChromeAXNodeId");
  }
}

base::Value::Dict AXTreeFormatterMac::BuildTree(
    AXPlatformNodeDelegate* root) const {
  DCHECK(root);
  return BuildTree(root->GetNativeViewAccessible());
}

base::Value::Dict AXTreeFormatterMac::BuildTreeForSelector(
    const AXTreeSelector& selector) const {
  base::apple::ScopedCFTypeRef<AXUIElementRef> node;
  std::tie(node, std::ignore) = FindAXUIElement(selector);
  if (!node) {
    return base::Value::Dict();
  }
  return BuildTreeForAXUIElement(node.get());
}

base::Value::Dict AXTreeFormatterMac::BuildTreeForAXUIElement(
    AXUIElementRef node) const {
  return BuildTree((__bridge id)node);
}

base::Value::Dict AXTreeFormatterMac::BuildTree(const id root) const {
  DCHECK(root);

  AXTreeIndexerMac indexer(root);
  base::Value::Dict dict;

  AXElementWrapper ax_element(root);
  NSPoint position = ax_element.Position();
  NSSize size = ax_element.Size();
  NSRect rect = NSMakeRect(position.x, position.y, size.width, size.height);

  RecursiveBuildTree(ax_element, rect, &indexer, &dict);

  return dict;
}

std::string AXTreeFormatterMac::EvaluateScript(
    const AXTreeSelector& selector,
    const AXInspectScenario& scenario) const {
  base::apple::ScopedCFTypeRef<AXUIElementRef> root;
  std::tie(root, std::ignore) = FindAXUIElement(selector);
  if (!root)
    return "";

  std::string result =
      EvaluateScript((__bridge id)root.get(), scenario.script_instructions, 0,
                     scenario.script_instructions.size());

  return result;
}

std::string AXTreeFormatterMac::EvaluateScript(
    AXPlatformNodeDelegate* root,
    const std::vector<AXScriptInstruction>& instructions,
    size_t start_index,
    size_t end_index) const {
  return EvaluateScript(root->GetNativeViewAccessible(), instructions,
                        start_index, end_index);
}

std::string AXTreeFormatterMac::EvaluateScript(
    id platform_root,
    const std::vector<AXScriptInstruction>& instructions,
    size_t start_index,
    size_t end_index) const {
  base::Value::List scripts;
  AXTreeIndexerMac indexer(platform_root);
  std::map<std::string, id> storage;
  AXCallStatementInvoker invoker(&indexer, &storage);
  for (size_t index = start_index; index < end_index; index++) {
    if (instructions[index].IsComment()) {
      scripts.Append(instructions[index].AsComment());
      continue;
    }

    DCHECK(instructions[index].IsScript());
    const AXPropertyNode& property_node = instructions[index].AsScript();
    AXOptionalNSObject value = invoker.Invoke(property_node);
    if (value.IsUnsupported()) {
      continue;
    }

    base::Value result;
    if (value.IsError()) {
      result = value.HasStateText()
                   ? base::Value(kFailedPrefix + value.StateText())
                   : base::Value(kFailedToParseError);
    } else if (value.IsNotApplicable()) {
      result = base::Value(kNotApplicable);
    } else {
      result = AXNSObjectToBaseValue(*value, &indexer);
    }

    scripts.Append(property_node.ToString() + "=" + AXFormatValue(result));
  }

  std::string contents;
  for (const base::Value& script : scripts) {
    std::string line;
    WriteAttribute(true, script.GetString(), &line);
    contents += line + "\n";
  }
  return contents;
}

base::Value::Dict AXTreeFormatterMac::BuildNode(
    AXPlatformNodeDelegate* node) const {
  DCHECK(node);
  return BuildNode(node->GetNativeViewAccessible());
}

base::Value::Dict AXTreeFormatterMac::BuildNode(const id node) const {
  DCHECK(node);

  AXTreeIndexerMac indexer(node);
  base::Value::Dict dict;

  AXElementWrapper ax_element(node);
  NSPoint position = ax_element.Position();
  NSSize size = ax_element.Size();
  NSRect rect = NSMakeRect(position.x, position.y, size.width, size.height);

  AddProperties(ax_element, rect, &indexer, &dict);
  return dict;
}

void AXTreeFormatterMac::RecursiveBuildTree(const AXElementWrapper& ax_element,
                                            const NSRect& root_rect,
                                            const AXTreeIndexerMac* indexer,
                                            base::Value::Dict* dict) const {
  AXPlatformNodeDelegate* platform_node = ax_element.IsNSAccessibilityElement()
                                              ? [ax_element.AsId() nodeDelegate]
                                              : nullptr;

  if (platform_node && !ShouldDumpNode(*platform_node))
    return;

  AddProperties(ax_element, root_rect, indexer, dict);
  if (platform_node && !ShouldDumpChildren(*platform_node))
    return;

  NSArray* children = ax_element.Children();
  base::Value::List child_dict_list;
  for (id child in children) {
    base::Value::Dict child_dict;
    RecursiveBuildTree(AXElementWrapper{child}, root_rect, indexer,
                       &child_dict);
    child_dict_list.Append(std::move(child_dict));
  }
  dict->Set(kChildrenDictAttr, std::move(child_dict_list));
}

void AXTreeFormatterMac::AddProperties(const AXElementWrapper& ax_element,
                                       const NSRect& root_rect,
                                       const AXTreeIndexerMac* indexer,
                                       base::Value::Dict* dict) const {
  // Chromium special attributes.
  dict->Set(kLocalPositionDictAttr,
            PopulateLocalPosition(ax_element, root_rect));

  // Dump all attributes if match-all filter is specified.
  if (HasMatchAllPropertyFilter()) {
    NSArray* attributes = ax_element.AttributeNames();
    for (NSString* attribute : attributes) {
      dict->SetByDottedPath(
          base::SysNSStringToUTF8(attribute),
          AXNSObjectToBaseValue(*ax_element.GetAttributeValue(attribute),
                                indexer));
    }
    return;
  }

  // Otherwise dump attributes matching allow filters only.
  std::string line_index = indexer->IndexBy(ax_element.AsId());
  for (const AXPropertyNode& property_node :
       PropertyFilterNodesFor(line_index)) {
    AXCallStatementInvoker invoker(ax_element.AsId(), indexer);
    AXOptionalNSObject value = invoker.Invoke(property_node);
    if (value.IsNotApplicable() || value.IsUnsupported()) {
      continue;
    }
    if (value.IsError()) {
      dict->SetByDottedPath(property_node.original_property,
                            base::Value(kFailedToParseError));
      continue;
    }
    dict->SetByDottedPath(property_node.original_property,
                          AXNSObjectToBaseValue(*value, indexer));
  }
}

base::Value::Dict AXTreeFormatterMac::PopulateLocalPosition(
    const AXElementWrapper& ax_element,
    const NSRect& root_rect) const {
  // The NSAccessibility position of an object is in global coordinates and
  // based on the lower-left corner of the object. To make this easier and
  // less confusing, convert it to local window coordinates using the top-left
  // corner when dumping the position.
  int root_top = -static_cast<int>(root_rect.origin.y + root_rect.size.height);
  int root_left = static_cast<int>(root_rect.origin.x);

  NSPoint node_position = ax_element.Position();
  NSSize node_size = ax_element.Size();

  return AXNSPointToBaseValue(NSMakePoint(
      static_cast<int>(node_position.x - root_left),
      static_cast<int>(-node_position.y - node_size.height - root_top)));
}

std::string AXTreeFormatterMac::ProcessTreeForOutput(
    const base::Value::Dict& dict) const {
  const std::string* error_value = dict.FindString("error");
  if (error_value)
    return *error_value;

  std::string line;

  // AXRole and AXSubrole have own formatting and should be listed upfront.
  std::string role_attr = base::SysNSStringToUTF8(NSAccessibilityRoleAttribute);
  const std::string* value = dict.FindString(role_attr);
  if (value) {
    WriteAttribute(true, *value, &line);
  }
  std::string subrole_attr =
      base::SysNSStringToUTF8(NSAccessibilitySubroleAttribute);
  value = dict.FindString(subrole_attr);
  if (value) {
    WriteAttribute(
        false,
        base::StringPrintf("%s=%s", subrole_attr.c_str(), value->c_str()),
        &line);
  }

  // Expose all other attributes.
  for (auto item : dict) {
    if (item.second.is_string() &&
        (item.first == role_attr || item.first == subrole_attr)) {
      continue;
    }

    // Special case: children.
    // Children are used to generate the tree
    // itself, thus no sense to expose them on each node.
    if (item.first == kChildrenDictAttr) {
      continue;
    }

    // Write formatted value.
    std::string formatted_value = AXFormatValue(item.second);
    WriteAttribute(false,
                   base::StringPrintf("%s=%s", item.first.c_str(),
                                      formatted_value.c_str()),
                   &line);
  }

  return line;
}

}  // namespace ui

#pragma clang diagnostic pop
