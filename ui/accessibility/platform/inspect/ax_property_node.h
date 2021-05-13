// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_PROPERTY_NODE_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_PROPERTY_NODE_H_

#include <string>
#include <vector>

#include "ui/accessibility/ax_export.h"

namespace base {
template <typename T>
class Optional;
}

namespace ui {

struct AXPropertyFilter;

// Property node is a tree-like structure, representing a property or collection
// of properties and its invocation parameters. A collection of properties is
// specified by putting a wildcard into a property name, for exampe, AXRole*
// will match both AXRole and AXRoleDescription properties. Parameters of a
// property are given in parentheses like a conventional function call, for
// example, AXCellForColumnAndRow([0, 0]) will call AXCellForColumnAndRow
// parameterized property for column/row 0 indexes.
class AX_EXPORT AXPropertyNode final {
 public:
  // Parses a property node from a property filter.
  static AXPropertyNode From(const AXPropertyFilter& filter);

  AXPropertyNode();
  AXPropertyNode(AXPropertyNode&&);
  ~AXPropertyNode();

  AXPropertyNode& operator=(AXPropertyNode&& other);
  explicit operator bool() const;

  // Key name in case of { key: value } dictionary.
  std::string key;

  // An object the property should be called for, designated by a line number
  // in accessible tree the object is located at. For example, :1 indicates
  // that the property should be called for an object located at first line.
  std::string target;

  // Value or a property name, for example 3 or AXLineForIndex
  std::string name_or_value;

  // Parameters if it's a property, for example, it is a vector of a single
  // value 3 in case of AXLineForIndex(3)
  std::vector<AXPropertyNode> parameters;

  // Used to store the origianl unparsed property including invocation
  // parameters if any.
  std::string original_property;

  // The list of line indexes of accessible objects the property is allowed to
  // be called for, used if no property target is provided.
  std::vector<std::string> line_indexes;

  bool IsMatching(const std::string& pattern) const;

  // Argument conversion methods.
  bool IsArray() const;
  bool IsDict() const;
  base::Optional<int> AsInt() const;
  const AXPropertyNode* FindKey(const char* refkey) const;
  base::Optional<std::string> FindStringKey(const char* refkey) const;
  base::Optional<int> FindIntKey(const char* key) const;

  std::string ToString() const;

 private:
  using iterator = std::string::const_iterator;

  explicit AXPropertyNode(iterator key_begin,
                          iterator key_end,
                          const std::string&);
  AXPropertyNode(iterator begin, iterator end);
  AXPropertyNode(iterator key_begin,
                 iterator key_end,
                 iterator value_begin,
                 iterator value_end);

  // Helper to set context and name.
  void Set(iterator begin, iterator end);

  // Builds a property node struct for a string of NAME(ARG1, ..., ARGN) format,
  // where each ARG is a scalar value or a string of the same format.
  static iterator Parse(AXPropertyNode* node, iterator begin, iterator end);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_PROPERTY_NODE_H_
