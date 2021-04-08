// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace ui {

AXInspectScenario::AXInspectScenario(
    const std::vector<ui::AXPropertyFilter>& default_filters)
    : property_filters(default_filters) {}
AXInspectScenario::AXInspectScenario(AXInspectScenario&&) = default;
AXInspectScenario::~AXInspectScenario() = default;
AXInspectScenario& AXInspectScenario::operator=(AXInspectScenario&&) = default;

// static
AXInspectScenario AXInspectScenario::From(
    const std::string& directive_prefix,
    const std::vector<std::string>& lines,
    const std::vector<ui::AXPropertyFilter>& default_filters) {
  AXInspectScenario scenario(default_filters);
  for (const std::string& line : lines) {
    // Directives have format of @directive:value.
    if (!base::StartsWith(line, "@")) {
      continue;
    }

    auto directive_end_pos = line.find_first_of(':');
    if (directive_end_pos == std::string::npos) {
      continue;
    }

    Directive directive =
        ParseDirective(directive_prefix, line.substr(0, directive_end_pos));
    if (directive == kNone)
      continue;

    std::string value = line.substr(directive_end_pos + 1);
    scenario.ProcessDirective(directive, value);
  }
  return scenario;
}

// static
AXInspectScenario::Directive AXInspectScenario::ParseDirective(
    const std::string& directive_prefix,
    const std::string& directive) {
  if (directive == "@NO-LOAD-EXPECTED")
    return kNoLoadExpected;
  if (directive == "@WAIT-FOR")
    return kWaitFor;
  if (directive == "@EXECUTE-AND-WAIT-FOR")
    return kExecuteAndWaitFor;
  if (directive == directive_prefix + "-RUN-UNTIL-EVENT")
    return kRunUntil;
  if (directive == "@DEFAULT-ACTION-ON")
    return kDefaultActionOn;
  if (directive == directive_prefix + "-ALLOW")
    return kPropertyFilterAllow;
  if (directive == directive_prefix + "-ALLOW-EMPTY")
    return kPropertyFilterAllowEmpty;
  if (directive == directive_prefix + "-DENY")
    return kPropertyFilterDeny;
  if (directive == directive_prefix + "-SCRIPT")
    return kScript;
  if (directive == directive_prefix + "-DENY-NODE")
    return kNodeFilter;

  return kNone;
}

void AXInspectScenario::ProcessDirective(Directive directive,
                                         const std::string& value) {
  switch (directive) {
    case kNoLoadExpected:
      no_load_expected.push_back(value);
      break;
    case kWaitFor:
      wait_for.push_back(value);
      break;
    case kExecuteAndWaitFor:
      execute.push_back(value);
      break;
    case kRunUntil:
      run_until.push_back(value);
      break;
    case kDefaultActionOn:
      default_action_on.push_back(value);
      break;
    case kPropertyFilterAllow:
      property_filters.emplace_back(value, AXPropertyFilter::ALLOW);
      break;
    case kPropertyFilterAllowEmpty:
      property_filters.emplace_back(value, AXPropertyFilter::ALLOW_EMPTY);
      break;
    case kPropertyFilterDeny:
      property_filters.emplace_back(value, AXPropertyFilter::DENY);
      break;
    case kScript:
      property_filters.emplace_back(value, AXPropertyFilter::SCRIPT);
      break;
    case kNodeFilter: {
      const auto& parts = base::SplitString(value, "=", base::TRIM_WHITESPACE,
                                            base::SPLIT_WANT_NONEMPTY);
      if (parts.size() == 2)
        node_filters.emplace_back(parts[0], parts[1]);
      else
        LOG(WARNING) << "Failed to parse node filter " << value;
      break;
    }
    default:
      NOTREACHED() << "Unrecognized " << directive << " directive";
      break;
  }
}

}  // namespace ui
