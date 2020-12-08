// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_property_node.h"

#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace ui {

// static
AXPropertyNode AXPropertyNode::From(const AXPropertyFilter& filter) {
  // Property invocation: property_str expected format is
  // prop_name or prop_name(arg1, ... argN).
  AXPropertyNode root;
  const std::string& property_str = filter.property_str;
  Parse(&root, property_str.begin(), property_str.end());

  AXPropertyNode* node = &root.parameters[0];

  // Expel a trailing wildcard if any.
  node->original_property =
      property_str.substr(0, property_str.find_last_of('*'));

  // Line indexes filter: filter_str expected format is
  // :line_num_1, ... :line_num_N, a comma separated list of line indexes
  // the property should be queried for. For example, ":1,:5,:7" indicates that
  // the property should called for objects placed on 1, 5 and 7 lines only.
  const std::string& filter_str = filter.filter_str;
  if (!filter_str.empty()) {
    node->line_indexes =
        base::SplitString(filter_str, std::string(1, ','),
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  }

  return std::move(*node);
}

AXPropertyNode::AXPropertyNode() = default;
AXPropertyNode::AXPropertyNode(AXPropertyNode&& o)
    : key(std::move(o.key)),
      target(std::move(o.target)),
      name_or_value(std::move(o.name_or_value)),
      parameters(std::move(o.parameters)),
      original_property(std::move(o.original_property)),
      line_indexes(std::move(o.line_indexes)) {}
AXPropertyNode::~AXPropertyNode() = default;

AXPropertyNode& AXPropertyNode::operator=(AXPropertyNode&& o) {
  key = std::move(o.key);
  target = std::move(o.target);
  name_or_value = std::move(o.name_or_value);
  parameters = std::move(o.parameters);
  original_property = std::move(o.original_property);
  line_indexes = std::move(o.line_indexes);
  return *this;
}

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

base::Optional<int> AXPropertyNode::AsInt() const {
  int value = 0;
  if (!base::StringToInt(name_or_value, &value)) {
    return base::nullopt;
  }
  return value;
}

const AXPropertyNode* AXPropertyNode::FindKey(const char* refkey) const {
  for (const auto& param : parameters) {
    if (param.key == refkey) {
      return &param;
    }
  }
  return nullptr;
}

base::Optional<std::string> AXPropertyNode::FindStringKey(
    const char* refkey) const {
  for (const auto& param : parameters) {
    if (param.key == refkey) {
      return param.name_or_value;
    }
  }
  return base::nullopt;
}

base::Optional<int> AXPropertyNode::FindIntKey(const char* refkey) const {
  for (const auto& param : parameters) {
    if (param.key == refkey) {
      return param.AsInt();
    }
  }
  return base::nullopt;
}

std::string AXPropertyNode::ToString() const {
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

  if (!key.empty()) {
    out += key + ": ";
  }

  if (!target.empty()) {
    out += target + '.';
  }
  out += name_or_value;
  if (parameters.size()) {
    out += '(';
    for (size_t i = 0; i < parameters.size(); i++) {
      if (i != 0) {
        out += ", ";
      }
      out += parameters[i].ToString();
    }
    out += ')';
  }
  return out;
}

// private
AXPropertyNode::AXPropertyNode(AXPropertyNode::iterator key_begin,
                               AXPropertyNode::iterator key_end,
                               const std::string& name_or_value)
    : key(key_begin, key_end) {
  Set(name_or_value.begin(), name_or_value.end());
}
AXPropertyNode::AXPropertyNode(AXPropertyNode::iterator begin,
                               AXPropertyNode::iterator end) {
  Set(begin, end);
}
AXPropertyNode::AXPropertyNode(AXPropertyNode::iterator key_begin,
                               AXPropertyNode::iterator key_end,
                               AXPropertyNode::iterator value_begin,
                               AXPropertyNode::iterator value_end)
    : key(key_begin, key_end), name_or_value(value_begin, value_end) {
  Set(value_begin, value_end);
}

void AXPropertyNode::Set(AXPropertyNode::iterator begin,
                         AXPropertyNode::iterator end) {
  AXPropertyNode::iterator dot_operator = std::find(begin, end, '.');
  if (dot_operator != end) {
    target = std::string(begin, dot_operator);
    name_or_value = std::string(dot_operator + 1, end);
  } else {
    name_or_value = std::string(begin, end);
  }
}

// private static
AXPropertyNode::iterator AXPropertyNode::Parse(AXPropertyNode* node,
                                               AXPropertyNode::iterator begin,
                                               AXPropertyNode::iterator end) {
  auto iter = begin;
  auto key_begin = end, key_end = end;
  while (iter != end) {
    // Subnode begins: create a new node, record its name and parse its
    // arguments.
    if (*iter == '(') {
      node->parameters.push_back(
          AXPropertyNode(key_begin, key_end, begin, iter));
      key_begin = key_end = end;
      begin = iter = Parse(&node->parameters.back(), ++iter, end);
      continue;
    }

    // Subnode begins: a special case for arrays, which have [arg1, ..., argN]
    // form.
    if (*iter == '[') {
      node->parameters.push_back(AXPropertyNode(key_begin, key_end, "[]"));
      key_begin = key_end = end;
      begin = iter = Parse(&node->parameters.back(), ++iter, end);
      continue;
    }

    // Subnode begins: a special case for dictionaries of {key1: value1, ...,
    // key2: value2} form.
    if (*iter == '{') {
      node->parameters.push_back(AXPropertyNode(key_begin, key_end, "{}"));
      key_begin = key_end = end;
      begin = iter = Parse(&node->parameters.back(), ++iter, end);
      continue;
    }

    // Subnode ends.
    if (*iter == ')' || *iter == ']' || *iter == '}') {
      if (begin != iter) {
        node->parameters.push_back(
            AXPropertyNode(key_begin, key_end, begin, iter));
        key_begin = key_end = end;
      }
      return ++iter;
    }

    // Dictionary key
    auto maybe_key_end = end;
    if (*iter == ':') {
      maybe_key_end = iter++;
    }

    // Skip spaces, adjust new node start.
    if (*iter == ' ') {
      if (maybe_key_end != end) {
        key_begin = begin;
        key_end = maybe_key_end;
      }
      begin = ++iter;
      continue;
    }

    // Subsequent scalar param case.
    if (*iter == ',' && begin != iter) {
      node->parameters.push_back(
          AXPropertyNode(key_begin, key_end, begin, iter));
      iter++;
      key_begin = key_end = end;
      begin = iter;
      continue;
    }

    iter++;
  }

  // Single scalar param case.
  if (begin != iter) {
    node->parameters.push_back(AXPropertyNode(begin, iter));
  }
  return iter;
}

}  // namespace ui
