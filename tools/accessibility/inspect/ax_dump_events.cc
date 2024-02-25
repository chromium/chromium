// Copyright 2017 The Chromium Authors
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
#include "build/build_config.h"
#include "tools/accessibility/inspect/ax_event_server.h"
#include "tools/accessibility/inspect/ax_utils.h"

using ui::AXTreeSelector;

namespace {

constexpr char kHelpSwitch[] = "help";

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
  tools::PrintHelpShared();
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

  std::optional<ui::AXInspectScenario> scenario =
      tools::ScenarioFromCommandLine(*command_line);
  if (!scenario) {
    return 1;
  }

  std::optional<AXTreeSelector> selector =
      tools::TreeSelectorFromCommandLine(*command_line);

  if (!selector || selector->empty()) {
    LOG(ERROR) << "* Error: no application was identified to dump events for. "
                  "Run with --help for help.";
    return 1;
  }
  base::AtExitManager exit_manager;
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);

  // The following code is temporary. The `pid` is set to ZERO for windows
  // because `selector->widget` is a HWND for windows, otherwise, it is a PID.
  // The window's code uses `selector->widget` to find the application later on.
  // A future patch will update mac and linux to use selector->widget and remove
  // the `pid` argument.
  unsigned int pid = 0;
#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_MAC)
  pid = selector->widget;
#endif
  const auto server =
      std::make_unique<tools::AXEventServer>(pid, *selector, *scenario);
  base::RunLoop().Run();
  return 0;
}
