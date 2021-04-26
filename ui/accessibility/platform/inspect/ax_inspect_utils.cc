// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_inspect_utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"

namespace ui {

constexpr char kConstValuePrefix[] = "_const_";
constexpr char kSetKeyPrefixDictAttr[] = "_setkey_";

std::string AX_EXPORT AXMakeConst(const std::string& value) {
  return kConstValuePrefix + value;
}

std::string AX_EXPORT AXMakeSetKey(const std::string& value) {
  return kSetKeyPrefixDictAttr + value;
}

std::string AXFormatValue(const base::Value& value) {
  // String.
  if (value.is_string()) {
    // Special handling for constants which are exposed as is, i.e. with no
    // quotation marks.
    std::string const_prefix = kConstValuePrefix;
    if (base::StartsWith(value.GetString(), const_prefix,
                         base::CompareCase::SENSITIVE)) {
      return value.GetString().substr(const_prefix.length());
    }
    return "'" + value.GetString() + "'";
  }

  // Integer.
  if (value.is_int()) {
    return base::NumberToString(value.GetInt());
  }

  // List: exposed as [value1, ..., valueN];
  if (value.is_list()) {
    std::string output;
    for (const auto& item : value.GetList()) {
      if (!output.empty()) {
        output += ", ";
      }
      output += AXFormatValue(item);
    }
    return "[" + output + "]";
  }

  // Dictionary. Exposed as {key1: value1, ..., keyN: valueN}. Set-like
  // dictionary is exposed as {value1, ..., valueN}.
  if (value.is_dict()) {
    const std::string setkey_prefix(kSetKeyPrefixDictAttr);
    std::string output;
    for (const auto& item : value.DictItems()) {
      if (!output.empty()) {
        output += ", ";
      }
      // Some of the dictionary's keys should not be appended to the output, so
      // that the dictionary can also be used as a set. Such keys start with
      // the _setkey_ prefix.
      if (base::StartsWith(item.first, setkey_prefix,
                           base::CompareCase::SENSITIVE)) {
        output += AXFormatValue(item.second);
      } else {
        output += item.first + ": " + AXFormatValue(item.second);
      }
    }
    return "{" + output + "}";
  }
  return "";
}

}  // namespace ui
