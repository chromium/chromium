// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/debug/debugger_utils.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"

namespace views {
namespace debug {

namespace {

using AttributeStrings = std::vector<std::string>;

constexpr int kElementIndent = 2;
constexpr int kAttributeIndent = 4;

std::string ToString(bool val) {
  return val ? "true" : "false";
}

std::string ToString(int val) {
  return base::NumberToString(val);
}

std::string ToString(ViewDebugWrapper::BoundsTuple bounds) {
  return base::StringPrintf("%d %d %dx%d", std::get<0>(bounds),
                            std::get<1>(bounds), std::get<2>(bounds),
                            std::get<3>(bounds));
}

// Adds attribute string of the form <attribute_name>="<attribute_value>".
template <typename T>
void AddAttributeString(AttributeStrings& attributes,
                        const std::string& name,
                        const T& value) {
  attributes.push_back(name + "\"=" + ToString(value) + "\"");
}

AttributeStrings GetAttributeStrings(ViewDebugWrapper* view) {
  AttributeStrings attributes;
  AddAttributeString(attributes, "bounds", view->GetBounds());
  AddAttributeString(attributes, "enabled", view->GetNeedsLayout());
  AddAttributeString(attributes, "id", view->GetID());
  AddAttributeString(attributes, "needs-layout", view->GetNeedsLayout());
  AddAttributeString(attributes, "visible", view->GetVisible());
  return attributes;
}

std::string GetPaddedLine(int current_depth, bool attribute_line = false) {
  const int padding = attribute_line
                          ? current_depth * kElementIndent + kAttributeIndent
                          : current_depth * kElementIndent;
  return std::string(padding, ' ');
}

void PrintViewHierarchyImpl(std::ostringstream* out,
                            ViewDebugWrapper* view,
                            int current_depth,
                            int target_depth,
                            size_t column_limit) {
  std::string line = GetPaddedLine(current_depth);

  line += "<" + view->GetViewClassName();

  for (const std::string& attribute_string : GetAttributeStrings(view)) {
    if (line.size() + attribute_string.size() + 1 > column_limit) {
      // If adding the attribute string would cause the line to exceed the
      // column limit, send the line to `out` and start a new line. The new line
      // should fit at least one attribute string even if it means exceeding the
      // column limit.
      *out << line << "\n";
      line = GetPaddedLine(current_depth, true) + attribute_string;
    } else {
      // Keep attribute strings on the existing line if it fits within the
      // column limit.
      line += " " + attribute_string;
    }
  }

  // Print children only if they exist and we are not yet at our target tree
  // depth.
  if (!view->GetChildren().empty() &&
      (target_depth == -1 || current_depth < target_depth)) {
    *out << line << ">\n";

    for (ViewDebugWrapper* child : view->GetChildren()) {
      PrintViewHierarchyImpl(out, child, current_depth + 1, target_depth,
                             column_limit);
    }

    line = GetPaddedLine(current_depth);
    *out << line << "</" << view->GetViewClassName() << ">\n";
  } else {
    // If no children are to be printed use a self closing tag to terminate the
    // View element.
    *out << line << " />\n";
  }
}

}  // namespace

void PrintViewHierarchy(std::ostringstream* out,
                        ViewDebugWrapper* view,
                        int depth,
                        size_t column_limit) {
  PrintViewHierarchyImpl(out, view, 0, depth, column_limit);
}

}  // namespace debug
}  // namespace views
