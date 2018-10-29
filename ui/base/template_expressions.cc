// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/template_expressions.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/values.h"
#include "net/base/escape.h"

namespace {
const char kLeader[] = "$i18n";
const size_t kLeaderSize = arraysize(kLeader) - 1;
const char kKeyOpen = '{';
const char kKeyClose = '}';

// Escape quotes and backslashes ('"\).
std::string PolymerParameterEscape(const std::string& in_string) {
  std::string out;
  out.reserve(in_string.size() * 2);
  for (const char c : in_string) {
    switch (c) {
      case '\\':
        out.append("\\\\");
        break;
      case '\'':
        out.append("\\'");
        break;
      case '"':
        out.append("&quot;");
        break;
      case ',':
        out.append("\\,");
        break;
      default:
        out += c;
    }
  }
  return out;
}

}  // namespace

namespace ui {

void TemplateReplacementsFromDictionaryValue(
    const base::DictionaryValue& dictionary,
    TemplateReplacements* replacements) {
  for (base::DictionaryValue::Iterator it(dictionary); !it.IsAtEnd();
       it.Advance()) {
    if (it.value().is_string()) {
      std::string str_value;
      if (it.value().GetAsString(&str_value))
        (*replacements)[it.key()] = str_value;
    }
  }
}

std::string ReplaceTemplateExpressions(
    base::StringPiece source,
    const TemplateReplacements& replacements) {
  std::string formatted;
  const size_t kValueLengthGuess = 16;
  formatted.reserve(source.length() + replacements.size() * kValueLengthGuess);
  // Two position markers are used as cursors through the |source|.
  // The |current_pos| will follow behind |next_pos|.
  size_t current_pos = 0;
  while (true) {
    size_t next_pos = source.find(kLeader, current_pos);

    if (next_pos == std::string::npos) {
      source.substr(current_pos).AppendToString(&formatted);
      break;
    }

    source.substr(current_pos, next_pos - current_pos)
        .AppendToString(&formatted);
    current_pos = next_pos + kLeaderSize;

    size_t context_end = source.find(kKeyOpen, current_pos);
    CHECK_NE(context_end, std::string::npos);
    std::string context;
    source.substr(current_pos, context_end - current_pos)
        .AppendToString(&context);
    current_pos = context_end + sizeof(kKeyOpen);

    size_t key_end = source.find(kKeyClose, current_pos);
    CHECK_NE(key_end, std::string::npos);

    std::string key =
        source.substr(current_pos, key_end - current_pos).as_string();
    CHECK(!key.empty());

    auto value = replacements.find(key);
    CHECK(value != replacements.end()) << "$i18n replacement key \"" << key
                                       << "\" not found";

    std::string replacement = value->second;
    if (context.empty()) {
      // Make the replacement HTML safe.
      replacement = net::EscapeForHTML(replacement);
    } else if (context == "Raw") {
      // Pass the replacement through unchanged.
    } else if (context == "Polymer") {
      // Escape quotes and backslash for '$i18nPolymer{}' use (i.e. quoted).
      replacement = PolymerParameterEscape(replacement);
    } else {
      CHECK(false) << "Unknown context " << context;
    }

    formatted.append(replacement);

    current_pos = key_end + sizeof(kKeyClose);
  }
  return formatted;
}

}  // namespace ui
