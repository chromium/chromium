// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Much of this logic is duplicated at
// tools/binary_size/libsupersize/function_signature.py.

#include <stddef.h>

#include <algorithm>
#include <deque>
#include <iostream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "tools/binary_size/libsupersize/caspian/function_signature.h"

namespace {
bool EndsWith(std::string_view str,
              std::string_view suffix,
              size_t pos = std::string_view::npos) {
  pos = std::min(pos, str.size());
  size_t span = suffix.size();
  return pos >= span && str.substr(pos - span, span) == suffix;
}

std::string_view Slice(std::string_view sv, size_t lo, size_t hi) {
  return sv.substr(lo, hi - lo);
}
}  // namespace

namespace caspian {
std::vector<std::string_view> SplitBy(std::string_view str, char delim) {
  std::vector<std::string_view> ret;
  while (true) {
    size_t pos = str.find(delim);
    ret.push_back(str.substr(0, pos));
    if (pos == std::string_view::npos) {
      break;
    }
    str = str.substr(pos + 1);
  }
  return ret;
}

std::tuple<std::string_view, std::string_view, std::string_view> ParseJava(
    std::string_view full_name,
    std::deque<std::string>* owned_strings) {
  // |owned_strings| is used as an allocator, the relative order of its
  // elements can be arbitrary.
  std::string maybe_member_type;
  size_t hash_idx = full_name.find('#');
  std::string_view full_class_name;
  std::string_view member;
  std::string_view member_type;
  if (hash_idx != std::string_view::npos) {
    // Parse an already parsed full_name.
    // Format: Class#symbol: type
    full_class_name = full_name.substr(0, hash_idx);
    size_t colon_idx = full_name.find(':');
    member = Slice(full_name, hash_idx + 1, colon_idx);
    if (colon_idx != std::string_view::npos) {
      member_type = full_name.substr(colon_idx);
    }
  } else {
    // Format: Class [returntype] functionName()
    std::vector<std::string_view> parts = SplitBy(full_name, ' ');
    full_class_name = parts[0];
    if (parts.size() >= 2) {
      member = parts.back();
    }
    if (parts.size() >= 3) {
      maybe_member_type = ": " + std::string(parts[1]);
      member_type = maybe_member_type;
    }
  }

  std::vector<std::string_view> split = SplitBy(full_class_name, '.');
  std::string_view short_class_name = split.back();

  if (member.empty()) {
    return std::make_tuple(full_name, full_name, short_class_name);
  }
  owned_strings->push_back(std::string(full_class_name) + std::string("#") +
                           std::string(member) + std::string(member_type));
  full_name = owned_strings->back();

  member = member.substr(0, member.find('('));

  owned_strings->push_back(std::string(short_class_name) + std::string("#") +
                           std::string(member));
  std::string_view name = owned_strings->back();

  owned_strings->push_back(std::string(full_class_name) + std::string("#") +
                           std::string(member));
  std::string_view template_name = owned_strings->back();

  return std::make_tuple(full_name, template_name, name);
}

size_t FindLastCharOutsideOfBrackets(std::string_view name,
                                     char target_char,
                                     size_t prev_idx) {
  int paren_balance_count = 0;
  int angle_balance_count = 0;
  std::string_view prefix = name.substr(0, prev_idx);
  while (true) {
    size_t idx = prefix.rfind(target_char);
    if (idx == std::string_view::npos) {
      return std::string_view::npos;
    }
    for (char c : prefix.substr(idx)) {
      switch (c) {
        case '<':
          angle_balance_count++;
          break;
        case '>':
          angle_balance_count--;
          break;
        case '(':
          paren_balance_count++;
          break;
        case ')':
          paren_balance_count--;
          break;
      }
    }
    if (angle_balance_count == 0 && paren_balance_count == 0) {
      return idx;
    }
    prefix = prefix.substr(0, idx);
  }
}

size_t FindReturnValueSpace(std::string_view name, size_t paren_idx) {
  size_t space_idx = paren_idx;
  // Special case: const cast operators (see tests).
  if (EndsWith(name, " const", paren_idx)) {
    space_idx = paren_idx - 6;
  }
  while (true) {
    space_idx = FindLastCharOutsideOfBrackets(name, ' ', space_idx);
    // Special cases: "operator new", "operator< <templ>", "operator<< <tmpl>".
    // No space is added for operator>><tmpl>.
    // Currently does not handle operator->, operator->*
    if (std::string_view::npos == space_idx) {
      break;
    }
    if (EndsWith(name, "operator", space_idx)) {
      space_idx -= 8;
    } else if (EndsWith(name, "operator<", space_idx)) {
      space_idx -= 9;
    } else if (EndsWith(name, "operator<<", space_idx)) {
      space_idx -= 10;
    } else {
      break;
    }
  }
  return space_idx;
}

std::string StripTemplateArgs(std::string_view name_view) {
  // TODO(jaspercb): Could pass in |owned_strings| to avoid this allocation.
  std::string name(name_view);
  size_t last_right_idx = std::string::npos;
  while (true) {
    last_right_idx = name.substr(0, last_right_idx).rfind('>');
    if (last_right_idx == std::string_view::npos) {
      return name;
    }
    size_t left_idx =
        FindLastCharOutsideOfBrackets(name, '<', last_right_idx + 1);
    if (left_idx != std::string_view::npos) {
      // Leave in empty <>s to denote that it's a template.
      name = std::string(name.substr(0, left_idx + 1)) +
             std::string(name.substr(last_right_idx));
      last_right_idx = left_idx;
    }
  }
}

std::string NormalizeTopLevelGccLambda(std::string_view name,
                                       size_t left_paren_idx) {
  // cc::{lambda(PaintOp*)#63}::_FUN(cc:PaintOp*)
  // -> cc::$lambda#63(cc:PaintOp*)

  size_t left_brace_idx = name.find('{');
  if (left_brace_idx == std::string_view::npos) {
    exit(1);
  }
  size_t hash_idx = name.find('#', left_brace_idx + 1);
  if (hash_idx == std::string_view::npos) {
    exit(1);
  }
  size_t right_brace_idx = name.find('}', hash_idx + 1);
  if (right_brace_idx == std::string_view::npos) {
    exit(1);
  }
  std::string_view number = Slice(name, hash_idx + 1, right_brace_idx);

  std::string ret;
  ret += name.substr(0, left_brace_idx);
  ret += "$lambda#";
  ret += number;
  ret += name.substr(left_paren_idx);
  return ret;
}

std::string NormalizeTopLevelClangLambda(std::string_view name,
                                         size_t left_paren_idx) {
  // cc::$_21::__invoke(int) -> cc::$lambda#21(int)
  size_t dollar_idx = name.find('$');
  if (dollar_idx == std::string_view::npos) {
    exit(1);
  }
  size_t colon_idx = name.find(':', dollar_idx + 1);
  if (colon_idx == std::string_view::npos) {
    exit(1);
  }
  std::string_view number = Slice(name, dollar_idx + 2, colon_idx);

  std::string ret;
  ret += name.substr(0, dollar_idx);
  ret += "$lambda#";
  ret += number;
  ret += name.substr(left_paren_idx);
  return ret;
}

size_t FindParameterListParen(std::string_view name) {
  size_t start_idx = 0;
  int angle_balance_count = 0;
  int paren_balance_count = 0;
  while (true) {
    size_t idx = name.find('(', start_idx);
    if (idx == std::string_view::npos) {
      return std::string_view::npos;
    }
    for (char c : Slice(name, start_idx, idx)) {
      switch (c) {
        case '<':
          angle_balance_count++;
          break;
        case '>':
          angle_balance_count--;
          break;
        case '(':
          paren_balance_count++;
          break;
        case ')':
          paren_balance_count--;
          break;
      }
    }
    size_t operator_offset = Slice(name, start_idx, idx).find("operator<");
    if (operator_offset != std::string_view::npos) {
      if (name[start_idx + operator_offset + 9] == '<') {
        // Handle operator<<, <<=
        angle_balance_count -= 2;
      } else {
        // Handle operator<=
        angle_balance_count -= 1;
      }
    } else {
      operator_offset = Slice(name, start_idx, idx).find("operator>");
      if (operator_offset != std::string_view::npos) {
        if (name[start_idx + operator_offset + 9] == '>') {
          // Handle operator>>,>>=
          angle_balance_count += 2;
        } else {
          // Handle operator>=
          angle_balance_count += 1;
        }
      }
    }

    // Adjust paren
    if (angle_balance_count == 0 && paren_balance_count == 0) {
      // Special case: skip "(anonymous namespace)".
      if (name.substr(idx, 21) == "(anonymous namespace)") {
        start_idx = idx + 21;
        continue;
      }
      // Special case: skip "decltype (...)"
      // Special case: skip "{lambda(PaintOp*)#63}"
      if (idx && name[idx - 1] != ' ' && !EndsWith(name, "{lambda", idx)) {
        return idx;
      }
    }

    start_idx = idx + 1;
    paren_balance_count++;
  }
}

std::tuple<std::string_view, std::string_view, std::string_view> ParseCpp(
    std::string_view full_name,
    std::deque<std::string>* owned_strings) {
  std::string name;
  std::string_view name_view;
  size_t left_paren_idx = FindParameterListParen(full_name);
  if (left_paren_idx != std::string::npos && left_paren_idx > 0) {
    size_t right_paren_idx = full_name.rfind(')');
    if (right_paren_idx <= left_paren_idx) {
      std::cerr << "ParseCpp() received bad symbol: " << full_name << std::endl;
      exit(1);
    }
    size_t space_idx = FindReturnValueSpace(full_name, left_paren_idx);
    std::string name_no_params =
        std::string(Slice(full_name, space_idx + 1, left_paren_idx));
    // Special case for top-level lambdas.
    if (EndsWith(name_no_params, "}::_FUN")) {
      // Don't use |name_no_params| in here since prior _idx will be off if
      // there was a return value.
      owned_strings->push_back(
          NormalizeTopLevelGccLambda(full_name, left_paren_idx));
      return ParseCpp(owned_strings->back(), owned_strings);
    } else if (EndsWith(name_no_params, "::__invoke") &&
               name_no_params.find('$') != std::string::npos) {
      owned_strings->push_back(
          NormalizeTopLevelClangLambda(full_name, left_paren_idx));
      return ParseCpp(owned_strings->back(), owned_strings);
    }

    name = name_no_params + std::string(full_name.substr(right_paren_idx + 1));
    name_view = name;
    full_name = full_name.substr(space_idx + 1);
  } else {
    name_view = full_name;
  }

  owned_strings->emplace_back(name_view);
  std::string_view template_name = owned_strings->back();

  owned_strings->push_back(StripTemplateArgs(name_view));
  std::string_view returned_name = owned_strings->back();

  return std::make_tuple(full_name, template_name, returned_name);
}
}  // namespace caspian
