// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "tools/accessibility/inspect/ax_tree_server.h"
#include "tools/accessibility/inspect/ax_utils.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"

char kHelpSwitch[] = "help";

bool AXDumpTreeLogMessageHandler(int severity,
                                 const char* file,
                                 int line,
                                 size_t message_start,
                                 const std::string& str) {
  printf("%s", str.substr(message_start).c_str());
  return true;
}

void PrintHelp() {
  printf(
      "ax_dump_tree is a tool designed to dump platform accessible trees "
      "of running applications.\n");
  printf("\nusage: ax_dump_tree <options>\n");
  tools::PrintHelpShared();
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

  absl::optional<ui::AXInspectScenario> scenario =
      tools::ScenarioFromCommandLine(*command_line);
  if (!scenario) {
    return 1;
  }

  absl::optional<ui::AXTreeSelector> selector =
      tools::TreeSelectorFromCommandLine(*command_line);
  if (!selector || selector->empty()) {
    LOG(ERROR)
        << "Error: no accessible tree to dump. Run with --help for help.";
    return 1;
  }

  auto server = absl::make_unique<content::AXTreeServer>(*selector, *scenario);
  return 0;
}
