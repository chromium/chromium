// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "tools/accessibility/inspect/ax_tree_server.h"
#include "tools/accessibility/inspect/ax_utils.h"

using ui::AXTreeSelector;

char kFiltersSwitch[] = "filters";
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
  printf("options:\n");
  tools::PrintHelpForTreeSelectors();
  printf(
      "  --filters\tfile containing property filters used to filter out\n"
      "  \t\taccessible tree, for example:\n"
      "  \t\t--filters=/absolute/path/to/filters/file\n");
  tools::PrintHelpFooter();
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

  base::FilePath filters_path =
      command_line->GetSwitchValuePath(kFiltersSwitch);
  if (filters_path.empty() && command_line->HasSwitch(kFiltersSwitch)) {
    LOG(ERROR) << "Error: empty filter path given. Run with --help for help.";
    return 0;
  }

  absl::optional<AXTreeSelector> selector =
      tools::TreeSelectorFromCommandLine(*command_line);

  if (selector && !selector->empty()) {
    auto server =
        absl::make_unique<content::AXTreeServer>(*selector, filters_path);
    return 0;
  }

  LOG(ERROR) << "Error: no accessible tree to dump. Run with --help for help.";
  return 1;
}
