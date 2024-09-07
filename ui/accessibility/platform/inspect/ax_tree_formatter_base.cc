// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

#include "base/containers/contains.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
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

bool AXTreeFormatterBase::ShouldDumpNode(
    const AXPlatformNodeDelegate& node) const {
  for (const std::pair<ax::mojom::StringAttribute, std::string>&
           string_attribute : node.GetStringAttributes()) {
    if (string_attribute.second.find(kSkipString) != std::string::npos)
      return false;
  }
  return true;
}

bool AXTreeFormatterBase::ShouldDumpChildren(
    const AXPlatformNodeDelegate& node) const {
  for (const std::pair<ax::mojom::StringAttribute, std::string>&
           string_attribute : node.GetStringAttributes()) {
    if (string_attribute.second.find(kSkipChildren) != std::string::npos)
      return false;
  }
  return true;
}

std::string AXTreeFormatterBase::Format(AXPlatformNodeDelegate* root) const {
  DCHECK(root);
  return FormatTree(BuildTree(root));
}

std::string AXTreeFormatterBase::FormatNode(
    AXPlatformNodeDelegate* node) const {
  return FormatTree(BuildNode(node));
}

base::Value::Dict AXTreeFormatterBase::BuildNode(
    AXPlatformNodeDelegate* node) const {
  return base::Value::Dict();
}

std::string AXTreeFormatterBase::FormatTree(
    const base::Value::Dict& dict) const {
  std::string contents;
  RecursiveFormatTree(dict, &contents);
  return contents;
}

base::Value::Dict AXTreeFormatterBase::BuildTreeForNode(AXNode* root) const {
  NOTREACHED_IN_MIGRATION()
      << "Only supported when called on AccessibilityTreeFormatterBlink.";
  return base::Value::Dict();
}

std::string AXTreeFormatterBase::EvaluateScript(
    const AXTreeSelector& selector,
    const AXInspectScenario& scenario) const {
  NOTIMPLEMENTED();
  return {};
}

std::string AXTreeFormatterBase::EvaluateScript(
    AXPlatformNodeDelegate* root,
    const std::vector<AXScriptInstruction>& instructions,
    size_t start_index,
    size_t end_index) const {
  NOTREACHED_IN_MIGRATION() << "Not implemented";
  return {};
}

void AXTreeFormatterBase::RecursiveFormatTree(const base::Value::Dict& dict,
                                              std::string* contents,
                                              int depth) const {
  // Check dictionary against node filters, may require us to skip this node
  // and its children.
  if (MatchesNodeFilters(dict))
    return;

  if (dict.empty())
    return;

  std::string indent = std::string(depth * kIndentSymbolCount, kIndentSymbol);
  std::string line = indent + ProcessTreeForOutput(dict);

  // TODO(accessibility): This can be removed once the UIA tree formatter
  // can call ShouldDumpNode().
  if (line.find(kSkipString) != std::string::npos)
    return;

  // Normalize any Windows-style line endings by removing \r.
  base::RemoveChars(line, "\r", &line);

  // Replace literal newlines with "<newline>"
  base::ReplaceChars(line, "\n", "<newline>", &line);

  // Replace U+202f to ASCII SPACE
  base::ReplaceFirstSubstringAfterOffset(&line, 0, "\u202f", " ");

  *contents += line + "\n";

  // TODO(accessibility): This can be removed once the UIA tree formatter
  // can call ShouldDumpChildren().
  if (line.find(kSkipChildren) != std::string::npos)
    return;

  const base::Value::List* children = dict.FindList(kChildrenDictAttr);
  if (children) {
    for (const auto& child : *children) {
      DCHECK(child.is_dict());
      RecursiveFormatTree(child.GetDict(), contents, depth + 1);
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
    AXTreeID tree_id,
    const std::vector<AXPropertyFilter>& property_filters) {
  NOTREACHED_IN_MIGRATION()
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
        !base::Contains(property_node.line_indexes, line_index)) {
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
  return AXTreeFormatter::MatchesPropertyFilters(property_filters_, text,
                                                 default_result);
}

bool AXTreeFormatterBase::MatchesNodeFilters(
    const base::Value::Dict& dict) const {
  return AXTreeFormatter::MatchesNodeFilters(node_filters_, dict);
}

std::string AXTreeFormatterBase::FormatCoordinates(
    const base::Value::Dict& dict,
    const std::string& name,
    const std::string& x_name,
    const std::string& y_name) const {
  int x = dict.FindInt(x_name).value_or(0);
  int y = dict.FindInt(y_name).value_or(0);
  return base::StringPrintf("%s=(%d, %d)", name.c_str(), x, y);
}

std::string AXTreeFormatterBase::FormatRectangle(
    const base::Value::Dict& dict,
    const std::string& name,
    const std::string& left_name,
    const std::string& top_name,
    const std::string& width_name,
    const std::string& height_name) const {
  int left = dict.FindInt(left_name).value_or(0);
  int top = dict.FindInt(top_name).value_or(0);
  int width = dict.FindInt(width_name).value_or(0);
  int height = dict.FindInt(height_name).value_or(0);
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
