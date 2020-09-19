// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/accessibility/inspect/ax_tree_server.h"

#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"

namespace content {

constexpr char kAllowOptEmptyStr[] = "@ALLOW-EMPTY:";
constexpr char kAllowOptStr[] = "@ALLOW:";
constexpr char kDenyOptStr[] = "@DENY:";

std::unique_ptr<base::DictionaryValue> BuildTreeForSelector(
    const AccessibilityTreeFormatter::TreeSelector& selector,
    AccessibilityTreeFormatter* formatter) {
  return formatter->BuildAccessibilityTreeForSelector(selector);
}

std::unique_ptr<base::DictionaryValue> BuildTreeForWindow(
    gfx::AcceleratedWidget widget,
    AccessibilityTreeFormatter* formatter) {
  return formatter->BuildAccessibilityTreeForWindow(widget);
}

AXTreeServer::AXTreeServer(
    const AccessibilityTreeFormatter::TreeSelector& selector,
    const base::FilePath& filters_path,
    bool use_json) {
  Run(base::BindOnce(&BuildTreeForSelector, selector), filters_path, use_json);
}

AXTreeServer::AXTreeServer(gfx::AcceleratedWidget widget,
                           const base::FilePath& filters_path,
                           bool use_json) {
  Run(base::BindOnce(&BuildTreeForWindow, widget), filters_path, use_json);
}

void AXTreeServer::Run(BuildTree build_tree,
                       const base::FilePath& filters_path,
                       bool use_json) {
  std::unique_ptr<AccessibilityTreeFormatter> formatter(
      AccessibilityTreeFormatter::Create());

  // Set filters.
  std::vector<AccessibilityTreeFormatter::PropertyFilter> filters =
      GetPropertyFilters(filters_path);
  if (filters.empty()) {
    LOG(ERROR) << "Failed to parse filters";
    return;
  }
  formatter->SetPropertyFilters(filters);

  // Get accessibility tree as a nested dictionary.
  std::unique_ptr<base::DictionaryValue> dict =
      std::move(build_tree).Run(formatter.get());
  if (!dict) {
    LOG(ERROR) << "Failed to get accessibility tree";
    return;
  }

  // Format the tree.
  Format(*formatter, *dict, use_json);
}

std::vector<AccessibilityTreeFormatter::PropertyFilter>
AXTreeServer::GetPropertyFilters(const base::FilePath& filters_path) {
  if (filters_path.empty()) {
    return {
      AccessibilityTreeFormatter::PropertyFilter(
          "*", AccessibilityTreeFormatter::PropertyFilter::ALLOW),
#if defined(OS_MAC)
      AccessibilityTreeFormatter::PropertyFilter(
          "children", AccessibilityTreeFormatter::PropertyFilter::DENY),
#endif
    };
  }

  std::string raw_filters_text;
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  if (!base::ReadFileToString(filters_path, &raw_filters_text)) {
    LOG(ERROR) << "Failed to open filters file " << filters_path
               << ". Note: path traversal components ('..') are not allowed "
                  "for security reasons";
    return {};
  }

  std::vector<AccessibilityTreeFormatter::PropertyFilter> filters;
  for (const std::string& line :
       base::SplitString(raw_filters_text, "\n", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_ALL)) {
    if (base::StartsWith(line, kAllowOptEmptyStr,
                         base::CompareCase::SENSITIVE)) {
      filters.emplace_back(
          line.substr(strlen(kAllowOptEmptyStr)),
          AccessibilityTreeFormatter::PropertyFilter::ALLOW_EMPTY);
    } else if (base::StartsWith(line, kAllowOptStr,
                                base::CompareCase::SENSITIVE)) {
      filters.emplace_back(line.substr(strlen(kAllowOptStr)),
                           AccessibilityTreeFormatter::PropertyFilter::ALLOW);
    } else if (base::StartsWith(line, kDenyOptStr,
                                base::CompareCase::SENSITIVE)) {
      filters.emplace_back(line.substr(strlen(kDenyOptStr)),
                           AccessibilityTreeFormatter::PropertyFilter::DENY);
    } else if (!line.empty()) {
      LOG(ERROR) << "Unrecognized filter instruction at line: " << line;
      return {};
    }
  }
  return filters;
}

void AXTreeServer::Format(AccessibilityTreeFormatter& formatter,
                          const base::DictionaryValue& dict,
                          bool use_json) {
  std::string accessibility_contents;

  // Format accessibility tree as JSON or text.
  if (use_json) {
    const std::unique_ptr<base::DictionaryValue> filtered_dict =
        formatter.FilterAccessibilityTree(dict);
    base::JSONWriter::WriteWithOptions(*filtered_dict,
                                       base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                       &accessibility_contents);
  } else {
    formatter.FormatAccessibilityTree(dict, &accessibility_contents);
  }

  // Write to console.
  printf("%s", accessibility_contents.c_str());
}

}  // namespace content
