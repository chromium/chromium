// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_PROPERTY_NODE_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_PROPERTY_NODE_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"

namespace ui {

struct AXPropertyFilter;

// Property node is a tree-like structure, representing a property or collection
// of properties and its invocation arguments. A collection of properties is
// specified by putting a wildcard into a property name, for exampe, AXRole*
// will match both AXRole and AXRoleDescription properties. Parameters of a
// property are given in parentheses like a conventional function call, for
// example, AXCellForColumnAndRow([0, 0]) will call AXCellForColumnAndRow
// parameterized property for column/row 0 indexes.
class COMPONENT_EXPORT(AX_PLATFORM) AXPropertyNode final {
 public:
  // Parses a property node from a string or a property filter.
  static AXPropertyNode From(const std::string& property,
                             const std::vector<std::string>& line_indexes = {});
  static AXPropertyNode From(const AXPropertyFilter& filter);

  AXPropertyNode();
  AXPropertyNode(AXPropertyNode&&);
  ~AXPropertyNode();

  AXPropertyNode& operator=(AXPropertyNode&& other);
  explicit operator bool() const;

  // Key name in case of { key: value } dictionary.
  std::string key;

  // Value or a property(method) name, for example 3 or AXLineForIndex
  std::string name_or_value;

  // Arguments if it's a method, for example, it is a vector of a single
  // value 3 in case of AXLineForIndex(3)
  std::vector<AXPropertyNode> arguments;

  // Next property node in a chain if any.
  std::unique_ptr<AXPropertyNode> next;

  // Rvalue if any.
  std::unique_ptr<AXPropertyNode> rvalue;

  // Used to store the original unparsed property including invocation
  // arguments if any.
  std::string original_property;

  // The list of line indexes of accessible objects the property is allowed to
  // be called for, used if no property target is provided.
  std::vector<std::string> line_indexes;

  template <class... Args>
  AXPropertyNode* ConnectTo(bool chained, Args&&... args) {
    return chained ? ChainToLastArgument(std::forward<Args>(args)...)
                   : AppendToArguments(std::forward<Args>(args)...);
  }

  template <class... Args>
  AXPropertyNode* AppendToArguments(Args&&... args) {
    arguments.emplace_back(std::forward<Args>(args)...);
    return &arguments.back();
  }
  template <class... Args>
  AXPropertyNode* ChainToLastArgument(Args&&... args) {
    auto* last = &arguments.back();
    while (last->next) {
      last = last->next.get();
    }
    last->next = std::make_unique<AXPropertyNode>(
        AXPropertyNode(std::forward<Args>(args)...));
    return last->next.get();
  }

  bool IsMatching(const std::string& pattern) const;

  // Argument conversion methods.
  bool IsTarget() const { return !!next; }
  bool IsArray() const;
  bool IsDict() const;
  std::optional<int> AsInt() const;
  std::string AsString() const;
  const AXPropertyNode* FindKey(const char* refkey) const;
  std::optional<std::string> FindStringKey(const char* refkey) const;
  std::optional<int> FindIntKey(const char* key) const;

  // Returns a string representation of the node.
  std::string ToString() const;

  // Returns a flat, single line string representing the node tree.
  std::string ToFlatString() const;

  // Returns a tree-like string representing the node tree.
  std::string ToTreeString(const std::string& indent = "") const;

  using iterator = std::string::const_iterator;

  explicit AXPropertyNode(iterator key_begin,
                          iterator key_end,
                          const std::string&);
  AXPropertyNode(iterator begin, iterator end);
  AXPropertyNode(iterator key_begin,
                 iterator key_end,
                 iterator value_begin,
                 iterator value_end);

 private:
  // Used by Parse to indicate a state the parser currently has.
  enum ParseState {
    kArgument,
    kChain,
  };

  // Builds a property node struct for a string of NAME(ARG1, ..., ARGN) format,
  // where each ARG is a scalar value or a string of the same format.
  static iterator Parse(AXPropertyNode* node, iterator begin, iterator end);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_PROPERTY_NODE_H_
