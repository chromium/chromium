// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <iostream>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/time/time.h"
#include "tools/mac/power/power_sampler/battery_sampler.h"
#include "tools/mac/power/power_sampler/csv_exporter.h"
#include "tools/mac/power/power_sampler/iopm_power_source_sampling_event_source.h"
#include "tools/mac/power/power_sampler/json_exporter.h"
#include "tools/mac/power/power_sampler/main_display_sampler.h"
#include "tools/mac/power/power_sampler/sample_counter.h"
#include "tools/mac/power/power_sampler/sampler.h"
#include "tools/mac/power/power_sampler/sampling_controller.h"
#include "tools/mac/power/power_sampler/timer_sampling_event_source.h"
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

constexpr char kSwitchHelp[] = "h";
constexpr char kSwitchSampleInterval[] = "sample-interval";
constexpr char kSwitchSampleCount[] = "sample-count";
constexpr char kSwitchJsonOutputFile[] = "json-output-file";
constexpr char kSwitchSampleOnNotification[] = "sample-on-notification";
constexpr char kUsageString[] = R"(Usage: power_sampler [options]

A tool that samples power-related metrics and states. The tool outputs samples
in CSV or JSON format.

Options:
  --sample-interval=<num>    Sample on a <num> second interval.
  --sample-on-notification   Sample on power manager notifications.
      Note that interval and event notifications are mutually exclusive.
  --sample-count=<num>       Collect <num> samples before exiting.
  --json-output-file=<path>  Produce JSON output to <path> before exit.
      By default output is in CSV format on STDOUT.
)";

// Prints main usage text.
void PrintUsage(const char* error) {
  if (error)
    std::cerr << "Error: " << error << std::endl << std::endl;
  std::cerr << kUsageString;
}

}  // namespace

// Status code, which can also be used as process exit code. Therefore
// success is explicitly 0.
enum StatusCode {
  kStatusSuccess = 0,
  kStatusUsage = 1,
  kStatusInvalidParam = 2,
  kStatusRuntimeError = 3,
};

int main(int argc, char** argv) {
  // Initialize infrastructure from base.
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  InitLogging();

  if (command_line.HasSwitch(kSwitchHelp)) {
    PrintUsage(nullptr);
    return kStatusUsage;
  }

  base::TimeDelta sampling_interval = base::Seconds(60);
  if (command_line.HasSwitch(kSwitchSampleInterval)) {
    if (command_line.HasSwitch(kSwitchSampleOnNotification)) {
      PrintUsage(
          "--sample-interval should not be specified with "
          "--sample-on-notification.");
      return kStatusInvalidParam;
    }

    std::string interval_seconds_switch =
        command_line.GetSwitchValueASCII(kSwitchSampleInterval);
    uint64_t interval_seconds = 0;
    if (!base::StringToUint64(interval_seconds_switch, &interval_seconds) ||
        interval_seconds < 1) {
      PrintUsage("sample-interval must be numeric and larger than 0.");
      return kStatusInvalidParam;
    }
    sampling_interval = base::Seconds(interval_seconds);
  }
  int64_t sample_count = -1;
  if (command_line.HasSwitch(kSwitchSampleCount)) {
    std::string sample_count_switch =
        command_line.GetSwitchValueASCII(kSwitchSampleCount);
    if (!base::StringToInt64(sample_count_switch, &sample_count) &&
        sample_count < 1) {
      PrintUsage("sample-count must be numeric and larger than 0.");
      return kStatusInvalidParam;
    }
  }

  base::FilePath json_output_file_path;
  if (command_line.HasSwitch(kSwitchJsonOutputFile)) {
    json_output_file_path =
        command_line.GetSwitchValuePath(kSwitchJsonOutputFile);
    if (json_output_file_path.empty()) {
      PrintUsage("must provide a file path for JSON output.");
      return kStatusInvalidParam;
    }
  }

  std::unique_ptr<power_sampler::SamplingEventSource> event_source;
  if (command_line.HasSwitch(kSwitchSampleOnNotification)) {
    event_source =
        std::make_unique<power_sampler::IOPMPowerSourceSamplingEventSource>();
  } else {
    event_source = std::make_unique<power_sampler::TimerSamplingEventSource>(
        sampling_interval);
  }

  base::SingleThreadTaskExecutor executor(base::MessagePumpType::NS_RUNLOOP);
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
  if (!json_output_file_path.empty()) {
    controller.AddMonitor(power_sampler::JsonExporter::Create(
        std::move(json_output_file_path), start_time));
  } else {
    controller.AddMonitor(power_sampler::CsvExporter::Create(
        start_time, base::File(STDOUT_FILENO)));
  }

  if (sample_count > 0) {
    controller.AddMonitor(
        std::make_unique<power_sampler::SampleCounter>(sample_count));
  }

  base::RunLoop run_loop;
  if (!event_source->Start(BindRepeating(
          [](power_sampler::SamplingController* controller,
             base::OnceClosure quit_closure) {
            if (controller->OnSamplingEvent()) {
              std::move(quit_closure).Run();
            }
          },
          base::Unretained(&controller), run_loop.QuitClosure()))) {
    PrintUsage("Could not start the sampling event source.");
    return kStatusRuntimeError;
  }

  controller.StartSession();

  run_loop.Run();

  controller.EndSession();

  return kStatusSuccess;
}
