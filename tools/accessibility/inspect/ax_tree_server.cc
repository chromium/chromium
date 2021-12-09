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
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"

using ui::AXTreeFormatter;
using ui::AXTreeSelector;

namespace content {

AXTreeServer::AXTreeServer(const AXTreeSelector& selector,
                           const ui::AXInspectScenario& scenario) {
  std::unique_ptr<AXTreeFormatter> formatter(
      AXInspectFactory::CreatePlatformFormatter());

  // Use optional filters with the default filter set
  formatter->SetPropertyFilters(scenario.property_filters,
                                AXTreeFormatter::kFiltersDefaultSet);

  // Get accessibility tree as a nested dictionary.
  base::Value dict = formatter->BuildTreeForSelector(selector);
  if (dict.DictEmpty()) {
    LOG(ERROR) << "Failed to get accessibility tree";
    return;
  }

  // Write to console.
  printf("%s", formatter->FormatTree(dict).c_str());
}

}  // namespace content
