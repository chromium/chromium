// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <iostream>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "tools/mac/power/power_sampler/battery_sampler.h"
#include "tools/mac/power/power_sampler/csv_exporter.h"
#include "tools/mac/power/power_sampler/main_display_sampler.h"
#include "tools/mac/power/power_sampler/sampler.h"
#include "tools/mac/power/power_sampler/sampling_controller.h"
#include "tools/mac/power/power_sampler/user_idle_level_sampler.h"

namespace {

void InitLogging() {
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  settings.log_file_path = nullptr;
  settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  settings.delete_old = logging::APPEND_TO_OLD_LOG_FILE;
  bool logging_res = logging::InitLogging(settings);
  CHECK(logging_res);
}

const char kSwitchHelp[] = "h";
const char kSwitchSampleInterval[] = "sample-interval";

// Prints main usage text.
void PrintUsage(std::ostream& err) {
  err << "Usage: powermetrics [-sample-interval=sample_interval]" << std::endl;
}

}  // namespace

// Status code, which can also be used as process exit code. Therefore
// success is explicitly 0.
enum StatusCode {
  kStatusSuccess = 0,
  kStatusUsage = 1,
  kStatusInvalidParam = 2,
};

int main(int argc, char** argv) {
  // Initialize infrastructure from base.
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  InitLogging();

  if (command_line.HasSwitch(kSwitchHelp)) {
    PrintUsage(std::cerr);
    return kStatusUsage;
  }

  base::TimeDelta sampling_interval = base::Seconds(60);
  if (command_line.HasSwitch(kSwitchSampleInterval)) {
    std::string interval_seconds_switch =
        command_line.GetSwitchValueASCII(kSwitchSampleInterval);
    uint64_t interval_seconds = 0;
    if (!base::StringToUint64(interval_seconds_switch, &interval_seconds)) {
      PrintUsage(std::cerr);
      return kStatusInvalidParam;
    }
    sampling_interval = base::Seconds(interval_seconds);
  }

  base::SingleThreadTaskExecutor executor(base::MessagePumpType::DEFAULT);
  power_sampler::SamplingController controller;

  std::unique_ptr<power_sampler::Sampler> sampler =
      power_sampler::MainDisplaySampler::Create();
  if (sampler)
    controller.AddSampler(std::move(sampler));

  sampler = power_sampler::BatterySampler::Create();
  if (sampler)
    controller.AddSampler(std::move(sampler));

  sampler = power_sampler::UserIdleLevelSampler::Create();
  if (sampler)
    controller.AddSampler(std::move(sampler));

  base::TimeTicks start_time = base::TimeTicks::Now();
  controller.AddMonitor(power_sampler::CsvExporter::Create(
      start_time, base::File(STDOUT_FILENO)));

  controller.StartSession();

  base::RunLoop run_loop;
  // TODO(siggi): Support battery notifications to drive OnSamplingEvent().
  base::RepeatingTimer timer(
      FROM_HERE, sampling_interval,
      BindRepeating(
          [](power_sampler::SamplingController* controller,
             base::OnceClosure quit_closure) {
            if (controller->OnSamplingEvent()) {
              std::move(quit_closure).Run();
            }
          },
          base::Unretained(&controller), run_loop.QuitClosure()));
  timer.Reset();

  run_loop.Run();

  controller.EndSession();

  // TODO(etiennep): Optionally export to a file.

  return kStatusSuccess;
}
