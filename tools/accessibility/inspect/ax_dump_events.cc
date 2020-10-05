// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "tools/accessibility/inspect/ax_event_server.h"
#include "tools/accessibility/inspect/ax_utils.h"

using ui::AXTreeSelector;

namespace {

constexpr char kPidSwitch[] = "pid";
constexpr char kHelpSwitch[] = "help";

// Convert from string to int, whether in 0x hex format or decimal format.
bool StringToInt(std::string str, int* result) {
  if (str.empty())
    return false;
  bool is_hex =
      str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X');
  return is_hex ? base::HexStringToInt(str, result)
                : base::StringToInt(str, result);
}

bool AXDumpEventsLogMessageHandler(int severity,
                                   const char* file,
                                   int line,
                                   size_t message_start,
                                   const std::string& str) {
  printf("%s", str.substr(message_start).c_str());
  return true;
}

void PrintHelp() {
  printf(
      "ax_dump_evemts is a tool designed to dump platform accessible events "
      "of running applications.\n");
  printf("\nusage: ax_dump_events <options>\n");
  printf("options:\n");
  printf(
      "  --pid\t\tprocess id of an application to dump accessible tree for\n");
  tools::PrintHelpForTreeSelectors();
}

}  // namespace

int main(int argc, char** argv) {
  logging::SetLogMessageHandler(AXDumpEventsLogMessageHandler);

  base::CommandLine::Init(argc, argv);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kHelpSwitch)) {
    PrintHelp();
    return 0;
  }

  const std::string pid_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kPidSwitch);
  const AXTreeSelector selector =
      tools::TreeSelectorFromCommandLine(command_line);

  if (pid_str.empty() && selector.empty()) {
    LOG(ERROR) << "* Error: no application was identified to dump events for. "
                  "Run with --help for help.";
    return 1;
  }

  int pid = 0;
  if (!pid_str.empty()) {
    if (!StringToInt(pid_str, &pid)) {
      LOG(ERROR) << "* Error: Could not convert process id to integer.";
      return 1;
    }
  }

  base::AtExitManager exit_manager;
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  const auto server = std::make_unique<tools::AXEventServer>(pid, selector);
  base::RunLoop().Run();
  return 0;
}
