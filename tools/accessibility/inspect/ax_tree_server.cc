// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/accessibility/inspect/ax_tree_server.h"

#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
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
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"
#include "ui/accessibility/platform/inspect/ax_script_instruction.h"

using ui::AXTreeFormatter;
using ui::AXTreeSelector;

namespace content {

AXTreeServer::AXTreeServer(const AXTreeSelector& selector,
                           const ui::AXInspectScenario& scenario,
                           ui::AXApiType::Type api)
    : error(false) {
  // If an API is not supplied, use the default API for this platform.
  std::unique_ptr<AXTreeFormatter> formatter =
      api != ui::AXApiType::kNone ? AXInspectFactory::CreateFormatter(api)
                                  : AXInspectFactory::CreatePlatformFormatter();

  // If there are script instructions, execute the script.
  if (!scenario.script_instructions.empty()) {
    std::string results = formatter->EvaluateScript(selector, scenario);

    if (results.empty()) {
      LOG(ERROR) << "Failed to find application or execute script.";
      error = true;
      return;
    }
    printf("%s", results.c_str());
    return;
  }

  // Otherwise, dump the tree.
  // Use user provided filters with the default filter set.
  std::vector<ui::AXPropertyFilter> property_filters_ext(
      {{"AXRoleDescription", ui::AXPropertyFilter::ALLOW}});
  property_filters_ext.insert(property_filters_ext.end(),
                              scenario.property_filters.begin(),
                              scenario.property_filters.end());

  formatter->SetPropertyFilters(property_filters_ext,
                                AXTreeFormatter::kFiltersDefaultSet);

  // Get accessibility tree as a nested dictionary.
  base::Value::Dict dict = formatter->BuildTreeForSelector(selector);

  if (dict.empty()) {
    LOG(ERROR) << "Failed to get accessibility tree.";
    error = true;
    return;
  }

  // Write to console.
  printf("%s", formatter->FormatTree(dict).c_str());
}

}  // namespace content
