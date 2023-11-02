// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"
#include "ui/accessibility/platform/inspect/ax_script_instruction.h"

namespace ui {

AXInspectScenario::AXInspectScenario(
    const std::vector<ui::AXPropertyFilter>& default_filters)
    : property_filters(default_filters) {}
AXInspectScenario::AXInspectScenario(AXInspectScenario&&) = default;
AXInspectScenario::~AXInspectScenario() = default;
AXInspectScenario& AXInspectScenario::operator=(AXInspectScenario&&) = default;

// static
absl::optional<AXInspectScenario> AXInspectScenario::From(
    const std::string& directive_prefix,
    const base::FilePath& scenario_path,
    const std::vector<ui::AXPropertyFilter>& default_filters) {
  std::vector<std::string> lines;
  std::string file_contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    if (!base::ReadFileToString(scenario_path, &file_contents)) {
      LOG(ERROR) << "Failed to open a file to extract an inspect scenario: "
                 << scenario_path;
      return absl::nullopt;
    }
  }

  // If the file is an HTML file, assume the directives are contained within
  // the first comment.
  if (scenario_path.Extension() == FILE_PATH_LITERAL(".html")) {
    size_t scenario_start = file_contents.find("<!--");
    size_t scenario_end = file_contents.find("-->", scenario_start);
    if (scenario_start != std::string::npos &&
        scenario_end != std::string::npos) {
      auto start = file_contents.begin() + scenario_start;
      auto end = start + (scenario_end - scenario_start);
      lines = base::SplitString(base::MakeStringPiece(start, end), "\n",
                                base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    }
  } else {
    // Otherwise, assume the whole file contains only directives
    lines = base::SplitString(file_contents, "\n", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_ALL);
  }
  return AXInspectScenario::From(directive_prefix, lines, default_filters);
}

// static
AXInspectScenario AXInspectScenario::From(
    const std::string& directive_prefix,
    const std::vector<std::string>& lines,
    const std::vector<ui::AXPropertyFilter>& default_filters) {
  AXInspectScenario scenario(default_filters);
  Directive directive = kNone;
  // Directives have format of @directive:value[..value], value per line.
  for (const std::string& line : lines) {
    // Treat empty line the multiline directive end with exception of script
    // directives which spans until a next directive.
    if (line.empty() && directive != kScript) {
      directive = kNone;
    }

    // Implicit directive case: use the most recent directive.
    if (!base::StartsWith(line, "@")) {
      if (directive != kNone) {
        std::string value(base::TrimWhitespaceASCII(line, base::TRIM_ALL));
        if (!value.empty())
          scenario.ProcessDirective(directive, value);
      }
      continue;
    }

    // Parse directive.
    auto directive_end_pos = line.find_first_of(':');
    if (directive_end_pos == std::string::npos)
      continue;

    directive =
        ParseDirective(directive_prefix, line.substr(0, directive_end_pos));
    if (directive == kNone)
      continue;

    std::string value = line.substr(directive_end_pos + 1);
    if (!value.empty())
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
  if (directive == "@WAIT-FOR" || directive == directive_prefix + "WAIT-FOR")
    return kWaitFor;
  if (directive == "@EXECUTE-AND-WAIT-FOR")
    return kExecuteAndWaitFor;
  if (directive == "@DEFAULT-ACTION-ON")
    return kDefaultActionOn;
  if (directive == directive_prefix + "ALLOW" || directive == "@ALLOW")
    return kPropertyFilterAllow;
  if (directive == directive_prefix + "ALLOW-EMPTY" ||
      directive == "@ALLOW-EMPTY") {
    return kPropertyFilterAllowEmpty;
  }
  if (directive == directive_prefix + "DENY" || directive == "@DENY")
    return kPropertyFilterDeny;
  if (directive == directive_prefix + "SCRIPT" || directive == "@SCRIPT")
    return kScript;
  if (directive == directive_prefix + "DENY-NODE" || directive == "@DENY-NODE")
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
      script_instructions.emplace_back(value);
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
