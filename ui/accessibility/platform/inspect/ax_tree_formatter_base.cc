// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"

namespace ui {

namespace {

const char kIndentSymbol = '+';
const int kIndentSymbolCount = 2;
const char kSkipString[] = "@NO_DUMP";
const char kSkipChildren[] = "@NO_CHILDREN_DUMP";

}  // namespace

AXTreeFormatterBase::AXTreeFormatterBase() = default;

AXTreeFormatterBase::~AXTreeFormatterBase() = default;

// static
const char AXTreeFormatterBase::kChildrenDictAttr[] = "children";
const char AXTreeFormatterBase::kScriptsDictAttr[] = "scripts";

std::string AXTreeFormatterBase::Format(AXPlatformNodeDelegate* root) const {
  DCHECK(root);
  return FormatTree(BuildTree(root));
}

std::string AXTreeFormatterBase::FormatTree(const base::Value& dict) const {
  std::string contents;

  // Format the tree.
  RecursiveFormatTree(dict, &contents);

  // Format scripts.
  const base::Value* scripts = dict.FindListKey(kScriptsDictAttr);
  if (!scripts)
    return contents;

  for (const base::Value& script : scripts->GetList()) {
    WriteAttribute(false, script.GetString(), &contents);
    contents += "\n";
  }

  return contents;
}

base::Value AXTreeFormatterBase::BuildTreeForNode(ui::AXNode* root) const {
  NOTREACHED()
      << "Only supported when called on AccessibilityTreeFormatterBlink.";
  return base::Value();
}

void AXTreeFormatterBase::RecursiveFormatTree(const base::Value& dict,
                                              std::string* contents,
                                              int depth) const {
  // Check dictionary against node filters, may require us to skip this node
  // and its children.
  if (MatchesNodeFilters(dict))
    return;

  std::string indent = std::string(depth * kIndentSymbolCount, kIndentSymbol);
  std::string line =
      indent + ProcessTreeForOutput(base::Value::AsDictionaryValue(dict));
  if (line.find(kSkipString) != std::string::npos)
    return;

  // Normalize any Windows-style line endings by removing \r.
  base::RemoveChars(line, "\r", &line);

  // Replace literal newlines with "<newline>"
  base::ReplaceChars(line, "\n", "<newline>", &line);

  *contents += line + "\n";
  if (line.find(kSkipChildren) != std::string::npos)
    return;

  const base::Value* children = dict.FindListPath(kChildrenDictAttr);
  if (children && !children->GetList().empty()) {
    for (const auto& child_dict : children->GetList()) {
      RecursiveFormatTree(child_dict, contents, depth + 1);
    }
  }
}

void AXTreeFormatterBase::SetPropertyFilters(
    const std::vector<AXPropertyFilter>& property_filters,
    PropertyFilterSet default_filter_set) {
  property_filters_.clear();
  if (default_filter_set == kFiltersDefaultSet) {
    AddDefaultFilters(&property_filters_);
  }
  property_filters_.insert(property_filters_.end(), property_filters.begin(),
                           property_filters.end());
}

void AXTreeFormatterBase::SetNodeFilters(
    const std::vector<AXNodeFilter>& node_filters) {
  node_filters_ = node_filters;
}

void AXTreeFormatterBase::set_show_ids(bool show_ids) {
  show_ids_ = show_ids;
}

std::string AXTreeFormatterBase::DumpInternalAccessibilityTree(
    ui::AXTreeID tree_id,
    const std::vector<AXPropertyFilter>& property_filters) {
  NOTREACHED()
      << "Only supported when called on AccessibilityTreeFormatterBlink.";
  return std::string("");
}

std::vector<AXPropertyNode> AXTreeFormatterBase::PropertyFilterNodesFor(
    const std::string& line_index) const {
  std::vector<AXPropertyNode> list;
  for (const auto& filter : property_filters_) {
    AXPropertyNode property_node = AXPropertyNode::From(filter);

    // Filter out if doesn't match line index (if specified).
    if (!property_node.line_indexes.empty() &&
        std::find(property_node.line_indexes.begin(),
                  property_node.line_indexes.end(),
                  line_index) == property_node.line_indexes.end()) {
      continue;
    }

    switch (filter.type) {
      case AXPropertyFilter::ALLOW_EMPTY:
      case AXPropertyFilter::ALLOW:
        list.push_back(std::move(property_node));
        break;
      case AXPropertyFilter::SCRIPT:
      case AXPropertyFilter::DENY:
        break;
      default:
        break;
    }
  }
  return list;
}

std::vector<AXPropertyNode> AXTreeFormatterBase::ScriptPropertyNodes() const {
  std::vector<AXPropertyNode> list;
  for (const auto& filter : property_filters_) {
    if (filter.type == AXPropertyFilter::SCRIPT) {
      list.push_back(AXPropertyNode::From(filter));
    }
  }
  return list;
}

bool AXTreeFormatterBase::HasMatchAllPropertyFilter() const {
  for (const auto& filter : property_filters_) {
    if (filter.type == AXPropertyFilter::ALLOW && filter.match_str == "*") {
      return true;
    }
  }
  return false;
}

bool AXTreeFormatterBase::MatchesPropertyFilters(const std::string& text,
                                                 bool default_result) const {
  return ui::AXTreeFormatter::MatchesPropertyFilters(property_filters_, text,
                                                     default_result);
}

bool AXTreeFormatterBase::MatchesNodeFilters(const base::Value& dict) const {
  return ui::AXTreeFormatter::MatchesNodeFilters(node_filters_, dict);
}

std::string AXTreeFormatterBase::FormatCoordinates(
    const base::Value& dict,
    const std::string& name,
    const std::string& x_name,
    const std::string& y_name) const {
  int x = dict.FindIntPath(x_name).value_or(0);
  int y = dict.FindIntPath(y_name).value_or(0);
  return base::StringPrintf("%s=(%d, %d)", name.c_str(), x, y);
}

std::string AXTreeFormatterBase::FormatRectangle(
    const base::Value& dict,
    const std::string& name,
    const std::string& left_name,
    const std::string& top_name,
    const std::string& width_name,
    const std::string& height_name) const {
  int left = dict.FindIntPath(left_name).value_or(0);
  int top = dict.FindIntPath(top_name).value_or(0);
  int width = dict.FindIntPath(width_name).value_or(0);
  int height = dict.FindIntPath(height_name).value_or(0);
  return base::StringPrintf("%s=(%d, %d, %d, %d)", name.c_str(), left, top,
                            width, height);
}

bool AXTreeFormatterBase::WriteAttribute(bool include_by_default,
                                         const std::string& attr,
                                         std::string* line) const {
  if (attr.empty())
    return false;
  if (!MatchesPropertyFilters(attr, include_by_default))
    return false;
  if (!line->empty())
    *line += " ";
  *line += attr;
  return true;
}

void AXTreeFormatterBase::AddPropertyFilter(
    std::vector<AXPropertyFilter>* property_filters,
    std::string filter,
    AXPropertyFilter::Type type) {
  property_filters->push_back(AXPropertyFilter(filter, type));
}

void AXTreeFormatterBase::AddDefaultFilters(
    std::vector<AXPropertyFilter>* property_filters) {}

}  // namespace ui
