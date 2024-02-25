// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_property_node.h"

#include <optional>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace ui {

// static
AXPropertyNode AXPropertyNode::From(
    const std::string& property,
    const std::vector<std::string>& line_indexes) {
  std::string::const_iterator lvalue_end(property.end()),
      rvalue_begin(property.end());

  // Find rvalue start if any. For example, in a statement
  // textarea.AXSelectedTextMarkerRange = textarea_range, textarea_range will be
  // rvalue. Do not confuse it with variable assignment ':=', for example,
  // textarea_range:= textarea.AXTextMarkerRangeForUIElement(textarea) which is
  // stored as a labelled lvalue.
  size_t index = property.find_last_of('=');
  if (index > 0 && index != std::string::npos) {
    if (property[index - 1] != ':') {
      lvalue_end = property.begin() + index - 1;
      index = property.find_first_not_of("= ", index);
      rvalue_begin = index == std::string::npos ? property.end()
                                                : property.begin() + index;
    }
  }

  // lvalue
  AXPropertyNode lvalue_root;
  Parse(&lvalue_root, property.begin(), lvalue_end);
  if (lvalue_root.arguments.size() == 0)  // Empty AXPropertyNode.
    return lvalue_root;

  AXPropertyNode* lvalue = &lvalue_root.arguments[0];
  lvalue->original_property = std::string(property.begin(), lvalue_end);
  lvalue->line_indexes = line_indexes;

  // rvalue if any
  if (lvalue_end != property.end()) {
    AXPropertyNode rvalue_root;
    Parse(&rvalue_root, rvalue_begin, property.end());

    // Connect rvalue to the latest lvalue in a chain.
    AXPropertyNode* last_in_chain = lvalue;
    for (; last_in_chain->next; last_in_chain = last_in_chain->next.get())
      ;

    // Use {std::make_unique_for_overwrite} once we allow C++20.
    last_in_chain->rvalue = std::unique_ptr<AXPropertyNode>(new AXPropertyNode);
    *(last_in_chain->rvalue) = std::move(rvalue_root.arguments[0]);
  }

  return std::move(*lvalue);
}

// static
AXPropertyNode AXPropertyNode::From(const AXPropertyFilter& filter) {
  // Expel an optional trailing wildcard which is used in tree formatter output
  // filtering.
  std::string property =
      filter.property_str.substr(0, filter.property_str.find_last_of('*'));

  // Line indexes filter: filter_str expected format is
  // :line_num_1, ... :line_num_N, a comma separated list of line indexes
  // the property should be queried for. For example, ":1,:5,:7" indicates that
  // the property should called for objects placed on 1, 5 and 7 lines only.
  return From(property, base::SplitString(
                            filter.filter_str, std::string(1, ','),
                            base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));
}

AXPropertyNode::AXPropertyNode() = default;
AXPropertyNode::AXPropertyNode(AXPropertyNode&& o) = default;
AXPropertyNode::~AXPropertyNode() = default;
AXPropertyNode& AXPropertyNode::operator=(AXPropertyNode&& o) = default;

AXPropertyNode::operator bool() const {
  return !name_or_value.empty();
}

bool AXPropertyNode::IsMatching(const std::string& pattern) const {
  // Looking for exact property match. Expel a trailing whildcard from
  // the property filter to handle filters like AXRole*.
  return name_or_value.compare(0, name_or_value.find_last_of('*'), pattern) ==
         0;
}

bool AXPropertyNode::IsArray() const {
  return name_or_value == "[]";
}

bool AXPropertyNode::IsDict() const {
  return name_or_value == "{}";
}

std::optional<int> AXPropertyNode::AsInt() const {
  int value = 0;
  if (!base::StringToInt(name_or_value, &value)) {
    return std::nullopt;
  }
  return value;
}

std::string AXPropertyNode::AsString() const {
  if (name_or_value.size() > 1 && name_or_value[0] == '\'' &&
      name_or_value[name_or_value.size() - 1] == '\'') {
    return name_or_value.substr(1, name_or_value.size() - 2);
  }
  return name_or_value;
}

const AXPropertyNode* AXPropertyNode::FindKey(const char* refkey) const {
  for (const auto& param : arguments) {
    if (param.key == refkey) {
      return &param;
    }
  }
  return nullptr;
}

std::optional<std::string> AXPropertyNode::FindStringKey(
    const char* refkey) const {
  for (const auto& param : arguments) {
    if (param.key == refkey) {
      return param.name_or_value;
    }
  }
  return std::nullopt;
}

std::optional<int> AXPropertyNode::FindIntKey(const char* refkey) const {
  for (const auto& param : arguments) {
    if (param.key == refkey) {
      return param.AsInt();
    }
  }
  return std::nullopt;
}

std::string AXPropertyNode::ToString() const {
  if (key.empty())
    return original_property;
  return key;
}

std::string AXPropertyNode::ToFlatString() const {
  std::string out;
  for (const auto& index : line_indexes) {
    if (!out.empty()) {
      out += ',';
    }
    out += index;
  }
  if (!out.empty()) {
    out += ';';
  }

  // Format a dictionary key or a variable.
  if (!key.empty()) {
    out += key + ": ";
  }

  out += name_or_value;
  if (arguments.size()) {
    out += '(';
    for (size_t i = 0; i < arguments.size(); i++) {
      if (i != 0) {
        out += ", ";
      }
      out += arguments[i].ToFlatString();
    }
    out += ')';
  }

  // Chains.
  if (next) {
    out += "." + next->ToFlatString();
  }

  // Rvalue.
  if (rvalue) {
    out += "=" + rvalue->ToFlatString();
  }

  return out;
}

std::string AXPropertyNode::ToTreeString(const std::string& indent) const {
  std::string out = indent;
  if (!key.empty()) {  // Key.
    out += key + ':';
  }
  out += name_or_value;    // Name or value.
  if (arguments.size()) {  // Arguments.
    out += "(\n";
    for (size_t i = 0; i < arguments.size(); i++) {
      if (i != 0) {
        out += ",\n";
      }
      out += arguments[i].ToTreeString(indent + "  ");
    }
    out += '\n' + indent + ')';
  }
  if (next) {  // Chains.
    out += ".\n" + next->ToTreeString(indent);
  }
  if (rvalue) {  // Rvalue.
    out += "=\n" + rvalue->ToTreeString(indent);
  }
  return out;
}

// private
AXPropertyNode::AXPropertyNode(AXPropertyNode::iterator key_begin,
                               AXPropertyNode::iterator key_end,
                               const std::string& name_or_value)
    : key(key_begin, key_end), name_or_value(name_or_value) {}
AXPropertyNode::AXPropertyNode(AXPropertyNode::iterator begin,
                               AXPropertyNode::iterator end)
    : name_or_value(begin, end) {}
AXPropertyNode::AXPropertyNode(AXPropertyNode::iterator key_begin,
                               AXPropertyNode::iterator key_end,
                               AXPropertyNode::iterator value_begin,
                               AXPropertyNode::iterator value_end)
    : key(key_begin, key_end), name_or_value(value_begin, value_end) {
}

// private static
AXPropertyNode::iterator AXPropertyNode::Parse(AXPropertyNode* node,
                                               AXPropertyNode::iterator begin,
                                               AXPropertyNode::iterator end) {
  auto iter = begin;
  auto key_begin = end, key_end = end, maybe_key_end = end;
  ParseState state = kArgument;
  while (iter != end) {
    // Subnode begins: create a new node, record its name and parse its
    // arguments.
    if (*iter == '(') {
      AXPropertyNode* child_node =
          node->ConnectTo(state & kChain, key_begin, key_end, begin, iter);

      key_begin = key_end = end;
      begin = iter = Parse(child_node, ++iter, end);
      continue;
    }

    // Subnode begins: a special case for arrays, which have [arg1, ..., argN]
    // form.
    if (*iter == '[') {
      // If a node ends by an array operator[], then chain them, for example,
      // AXChildren[0].
      if (begin != iter) {
        node->ConnectTo(state & kChain, key_begin, key_end, begin, iter);
        key_begin = key_end = end;
        state = kChain;
      }
      AXPropertyNode* child_node =
          node->ConnectTo(state & kChain, key_begin, key_end, "[]");
      key_begin = key_end = end;
      begin = iter = Parse(child_node, ++iter, end);
      continue;
    }

    // Subnode begins: a special case for dictionaries of {key1: value1, ...,
    // key2: value2} form.
    if (*iter == '{') {
      AXPropertyNode* child_node =
          node->AppendToArguments(key_begin, key_end, "{}");
      key_begin = key_end = end;
      begin = iter = Parse(child_node, ++iter, end);
      continue;
    }

    // Subnode ends.
    if (*iter == ')' || *iter == ']' || *iter == '}') {
      if (begin != iter) {
        node->ConnectTo(state & kChain, key_begin, key_end, begin, iter);
        key_begin = key_end = end;
      }
      state = kChain;
      return ++iter;
    }

    // Possible dictionary key or variable assignment end.
    if (*iter == ':') {
      maybe_key_end = iter++;
      continue;
    }

    // Possible variable assignment, move next.
    if (*iter == '=') {
      iter++;
      continue;
    }

    // Dictionary key or variable. If so, get a key and adjust a new node start.
    if (*iter == ' ') {
      if (maybe_key_end != end) {
        key_begin = begin;
        key_end = maybe_key_end;
        maybe_key_end = end;
      }
      begin = ++iter;
      continue;
    }

    // Not a dictionary key or a variable.
    maybe_key_end = end;

    // Call chains.
    if (*iter == '.') {
      if (begin != iter) {
        node->ConnectTo(state & kChain, key_begin, key_end, begin, iter);
        key_begin = key_end = end;
      }
      begin = ++iter;
      state = kChain;
      continue;
    }

    // Subsequent literal case.
    if (*iter == ',') {
      if (begin != iter) {
        node->ConnectTo(state & kChain, key_begin, key_end, begin, iter);
        key_begin = key_end = end;
      }
      state = kArgument;
      begin = ++iter;
      continue;
    }

    iter++;
  }

  // Single scalar param case.
  if (begin != iter) {
    node->ConnectTo(state & kChain, begin, iter);
  }
  return iter;
}

}  // namespace ui
