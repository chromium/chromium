// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"

#include "base/containers/fixed_flat_map.h"
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
    const std::vector<AXPropertyFilter>& default_filters)
    : property_filters(default_filters) {}
AXInspectScenario::AXInspectScenario(AXInspectScenario&&) = default;
AXInspectScenario::~AXInspectScenario() = default;
AXInspectScenario& AXInspectScenario::operator=(AXInspectScenario&&) = default;

// static
std::optional<AXInspectScenario> AXInspectScenario::From(
    const std::string& directive_prefix,
    const base::FilePath& scenario_path,
    const std::vector<AXPropertyFilter>& default_filters) {
  std::vector<std::string> lines;
  std::string file_contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    if (!base::ReadFileToString(scenario_path, &file_contents)) {
      LOG(ERROR) << "Failed to open a file to extract an inspect scenario: "
                 << scenario_path;
      return std::nullopt;
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
    const std::vector<AXPropertyFilter>& default_filters) {
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
        std::string_view trimmed_value =
            base::TrimWhitespaceASCII(line, base::TRIM_ALL);
        if (!trimmed_value.empty()) {
          scenario.ProcessDirective(directive, trimmed_value);
        }
      }
      continue;
    }

    // Parse directive.
    auto parts = base::SplitStringOnce(line, ':');
    if (!parts) {
      continue;
    }
    auto [name, value] = *parts;

    directive = ParseDirective(directive_prefix, name);
    if (directive == kNone)
      continue;

    if (!value.empty())
      scenario.ProcessDirective(directive, value);
  }
  return scenario;
}

// static
AXInspectScenario::Directive AXInspectScenario::ParseDirective(
    const std::string& directive_prefix,
    std::string_view directive) {
  static constexpr auto kMapping =
      base::MakeFixedFlatMap<std::string_view, Directive>(
          {{"NO-LOAD-EXPECTED", kNoLoadExpected},
           {"WAIT-FOR", kWaitFor},
           {"EXECUTE-AND-WAIT-FOR", kExecuteAndWaitFor},
           {"DEFAULT-ACTION-ON", kDefaultActionOn},
           {"ALLOW", kPropertyFilterAllow},
           {"ALLOW-EMPTY", kPropertyFilterAllowEmpty},
           {"DENY", kPropertyFilterDeny},
           {"SCRIPT", kScript},
           {"DENY-NODE", kNodeFilter}});

  if (!directive.starts_with('@')) {
    return kNone;
  }

  auto trimmed_directive = directive.starts_with(directive_prefix)
                               ? directive.substr(directive_prefix.size())
                               : directive.substr(1);

  auto it = kMapping.find(trimmed_directive);

  return it != kMapping.end() ? it->second : kNone;
}

void AXInspectScenario::ProcessDirective(Directive directive,
                                         std::string_view value) {
  switch (directive) {
    case kNoLoadExpected:
      no_load_expected.push_back(std::string(value));
      break;
    case kWaitFor:
      wait_for.push_back(std::string(value));
      break;
    case kExecuteAndWaitFor:
      execute.push_back(std::string(value));
      break;
    case kDefaultActionOn:
      default_action_on.push_back(std::string(value));
      break;
    case kPropertyFilterAllow:
      property_filters.emplace_back(std::string(value),
                                    AXPropertyFilter::ALLOW);
      break;
    case kPropertyFilterAllowEmpty:
      property_filters.emplace_back(std::string(value),
                                    AXPropertyFilter::ALLOW_EMPTY);
      break;
    case kPropertyFilterDeny:
      property_filters.emplace_back(std::string(value), AXPropertyFilter::DENY);
      break;
    case kScript:
      script_instructions.emplace_back(std::string(value));
      break;
    case kNodeFilter: {
      auto parts = base::SplitStringOnce(value, '=');
      if (parts) {
        node_filters.emplace_back(std::string(base::TrimWhitespaceASCII(
                                      parts->first, base::TRIM_ALL)),
                                  std::string(base::TrimWhitespaceASCII(
                                      parts->second, base::TRIM_ALL)));
      } else {
        LOG(WARNING) << "Failed to parse node filter " << value;
      }
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION() << "Unrecognized " << directive << " directive";
      break;
  }
}

}  // namespace ui
