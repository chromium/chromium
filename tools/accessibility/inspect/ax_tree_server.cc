// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/accessibility/inspect/ax_tree_server.h"

#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "content/public/browser/ax_inspect_factory.h"

using ui::AXTreeFormatter;
using ui::AXTreeSelector;

namespace content {

constexpr char kAllowOptEmptyStr[] = "@ALLOW-EMPTY:";
constexpr char kAllowOptStr[] = "@ALLOW:";
constexpr char kDenyOptStr[] = "@DENY:";

base::Value BuildTreeForSelector(const AXTreeSelector& selector,
                                 const AXTreeFormatter* formatter) {
  return formatter->BuildTreeForSelector(selector);
}

base::Value BuildTreeForWindow(gfx::AcceleratedWidget widget,
                               const AXTreeFormatter* formatter) {
  return formatter->BuildTreeForWindow(widget);
}

AXTreeServer::AXTreeServer(const AXTreeSelector& selector,
                           const base::FilePath& filters_path) {
  Run(base::BindOnce(&BuildTreeForSelector, selector), filters_path);
}

AXTreeServer::AXTreeServer(gfx::AcceleratedWidget widget,
                           const base::FilePath& filters_path) {
  Run(base::BindOnce(&BuildTreeForWindow, widget), filters_path);
}

void AXTreeServer::Run(BuildTree build_tree,
                       const base::FilePath& filters_path) {
  std::unique_ptr<AXTreeFormatter> formatter(
      AXInspectFactory::CreatePlatformFormatter());

  // Set filters.
  absl::optional<std::vector<ui::AXPropertyFilter>> filters =
      GetPropertyFilters(filters_path);
  if (!filters) {
    LOG(ERROR) << "Failed to parse filters2";
    return;
  }
  formatter->SetPropertyFilters(*filters,
                                ui::AXTreeFormatter::kFiltersDefaultSet);

  // Get accessibility tree as a nested dictionary.
  base::Value dict = std::move(build_tree).Run(formatter.get());
  if (dict.DictEmpty()) {
    LOG(ERROR) << "Failed to get accessibility tree";
    return;
  }

  // Write to console.
  printf("%s", formatter->FormatTree(dict).c_str());
}

absl::optional<std::vector<ui::AXPropertyFilter>>
AXTreeServer::GetPropertyFilters(const base::FilePath& filters_path) {
  std::vector<ui::AXPropertyFilter> filters;
  if (filters_path.empty()) {
    return filters;
  }

  std::string raw_filters_text;
  base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
  if (!base::ReadFileToString(filters_path, &raw_filters_text)) {
    LOG(ERROR) << "Failed to open filters file " << filters_path
               << ". Note: path traversal components ('..') are not allowed "
                  "for security reasons";
    return absl::nullopt;
  }

  for (const std::string& line :
       base::SplitString(raw_filters_text, "\n", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_ALL)) {
    if (base::StartsWith(line, kAllowOptEmptyStr,
                         base::CompareCase::SENSITIVE)) {
      filters.emplace_back(line.substr(strlen(kAllowOptEmptyStr)),
                           ui::AXPropertyFilter::ALLOW_EMPTY);
    } else if (base::StartsWith(line, kAllowOptStr,
                                base::CompareCase::SENSITIVE)) {
      filters.emplace_back(line.substr(strlen(kAllowOptStr)),
                           ui::AXPropertyFilter::ALLOW);
    } else if (base::StartsWith(line, kDenyOptStr,
                                base::CompareCase::SENSITIVE)) {
      filters.emplace_back(line.substr(strlen(kDenyOptStr)),
                           ui::AXPropertyFilter::DENY);
    } else if (!line.empty()) {
      LOG(ERROR) << "Unrecognized filter instruction at line: " << line;
      return absl::nullopt;
    }
  }
  return filters;
}

}  // namespace content
