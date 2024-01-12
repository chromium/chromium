// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <numeric>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "tools/accessibility/inspect/ax_tree_server.h"
#include "tools/accessibility/inspect/ax_utils.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"

constexpr char kApiSwitch[] = "api";
constexpr char kHelpSwitch[] = "help";

bool AXDumpTreeLogMessageHandler(int severity,
                                 const char* file,
                                 int line,
                                 size_t message_start,
                                 const std::string& str) {
  printf("%s", str.substr(message_start).c_str());
  return true;
}

// SupportedApis is a wrapper around content::AXInspectFactory::SupportedApis
// to filter out the Blink formatter option, as ax_dump_tree does not support
// outputting the chromium internal Blink tree. In the future we should support
// outputting the Blink tree when dumping chromium or chrome.
std::vector<ui::AXApiType::Type> SupportedApis() {
  std::vector<ui::AXApiType::Type> apis =
      content::AXInspectFactory::SupportedApis();
  std::vector<ui::AXApiType::Type> filter_apis;
  base::ranges::copy_if(
      apis, std::back_inserter(filter_apis),
      [](ui::AXApiType::Type t) { return t != ui::AXApiType::kBlink; });
  return filter_apis;
}

void PrintHelp() {
  std::vector<ui::AXApiType::Type> v = SupportedApis();
  std::string supported_apis = std::accumulate(
      begin(v), end(v), std::string{},
      [](std::string r, ui::AXApiType::Type p) {
        std::string comma;
        if (!r.empty()) {
          comma = ", ";
        }
        return std::move(r) + comma + static_cast<std::string>(p);
      });

  printf(
      "ax_dump_tree is a tool designed to dump platform accessible trees "
      "of running applications.\n");
  printf("\nusage: ax_dump_tree <options>\n");
  tools::PrintHelpShared();
  printf(
      "  --api\t\tAccessibility API for the current platform.\n"
      "  \t\tValid options are: %s\n",
      supported_apis.c_str());
}

int main(int argc, char** argv) {
  logging::SetLogMessageHandler(AXDumpTreeLogMessageHandler);

  base::AtExitManager at_exit_manager;

  base::CommandLine::Init(argc, argv);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kHelpSwitch)) {
    PrintHelp();
    return 0;
  }

  std::optional<ui::AXTreeSelector> selector =
      tools::TreeSelectorFromCommandLine(*command_line);

  if (!selector || selector->empty()) {
    LOG(ERROR)
        << "Error: no accessible tree to dump. Run with --help for help.";
    return 1;
  }

  std::string api_str = command_line->GetSwitchValueASCII(kApiSwitch);
  ui::AXApiType::Type api = ui::AXApiType::kNone;
  std::vector<ui::AXApiType::Type> apis = SupportedApis();
  if (!api_str.empty()) {
    api = ui::AXApiType::From(api_str);
    if (api == ui::AXApiType::kNone) {
      LOG(ERROR) << "Unknown API type: " << api_str;
      return 1;
    }
    if (!base::Contains(apis, api)) {
      LOG(ERROR) << "Unsupported API for this platform: "
                 << static_cast<std::string>(api);
      return 1;
    }
  }
  // Choose the default API if one is not specified.
  if (api == ui::AXApiType::kNone && !apis.empty())
    api = apis[0];

  std::optional<ui::AXInspectScenario> scenario =
      tools::ScenarioFromCommandLine(*command_line, api);
  if (!scenario) {
    return 1;
  }

  auto server =
      std::make_unique<content::AXTreeServer>(*selector, *scenario, api);

  if (server->error) {
    return 1;
  }
  return 0;
}
