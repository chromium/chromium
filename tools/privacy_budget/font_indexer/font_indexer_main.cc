// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_suite.h"
#include "base/test/test_timeouts.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "tools/privacy_budget/font_indexer/font_indexer.h"

namespace {

const char kHelpMsg[] = R"(
  font_indexer [--no-smart-skipping] [--more-slope-checks]

  --no-smart-skipping stops the tool from skipping checks along axes of
  variation when it appears the font does not varying along those axes. This
  will slow down the tool substantially, but may be more thorough if the checks
  are incorrect.

  --more-slope-checks gives more granular checking of different slopes. This
  will slow down the tool, but will give more results if a font with many
  slope variations is available.
)";

const char kNoSmartSkippingSwitch[] = "no-smart-skipping";
const char kMoreSlopeChecksSwitch[] = "more-slope-checks";

void PrintHelp() {
  printf("%s\n\n", kHelpMsg);
}

bool ShouldPrintHelpAndQuit(const base::CommandLine::StringVector& args,
                            const base::CommandLine::SwitchMap& switches) {
  if (args.size() != 0U || switches.size() > 2) {
    return true;
  }
  for (const auto& switch_entry : switches) {
    std::string switch_name = switch_entry.first;
    if (switch_name != kNoSmartSkippingSwitch &&
        switch_name != kMoreSlopeChecksSwitch) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = command_line.GetArgs();

  if (ShouldPrintHelpAndQuit(command_line.GetArgs(),
                             command_line.GetSwitches())) {
    PrintHelp();
    return 1;
  }

  // Initialize a test environment to satisfy the expectations of
  // content::GetFontListAsync().
  blink::ScopedUnittestsEnvironmentSetup testEnvironmentSetup(argc, argv);
  base::TestSuite testSuite(argc, argv);
  mojo::core::Init();
  base::TestIOThread testIoThread(base::TestIOThread::kAutoStart);
  mojo::core::ScopedIPCSupport ipcSupport(
      testIoThread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);
  TestTimeouts::Initialize();
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // Set up and run tool.
  privacy_budget::FontIndexer indexer;
  if (command_line.HasSwitch(kNoSmartSkippingSwitch)) {
    indexer.SetNoSmartSkipping();
  }
  if (command_line.HasSwitch(kMoreSlopeChecksSwitch)) {
    indexer.SetMoreSlopeChecks();
  }
  indexer.PrintAllFonts();

  return 0;
}
