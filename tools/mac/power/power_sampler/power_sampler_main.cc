// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <cstdio>
#include <iostream>
#include <memory>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/power_monitor/iopm_power_source_sampling_event_source.h"
#include "base/power_monitor/timer_sampling_event_source.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_executor.h"
#include "base/time/time.h"
#include "tools/mac/power/power_sampler/battery_sampler.h"
#include "tools/mac/power/power_sampler/csv_exporter.h"
#include "tools/mac/power/power_sampler/json_exporter.h"
#include "tools/mac/power/power_sampler/m1_sampler.h"
#include "tools/mac/power/power_sampler/main_display_sampler.h"
#include "tools/mac/power/power_sampler/resource_coalition_sampler.h"
#include "tools/mac/power/power_sampler/sample_counter.h"
#include "tools/mac/power/power_sampler/sampler.h"
#include "tools/mac/power/power_sampler/sampling_controller.h"
#include "tools/mac/power/power_sampler/smc_sampler.h"
#include "tools/mac/power/power_sampler/user_active_simulator.h"
#include "tools/mac/power/power_sampler/user_idle_level_sampler.h"

namespace {

void InitLogging() {
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  settings.delete_old = logging::APPEND_TO_OLD_LOG_FILE;
  bool logging_res = logging::InitLogging(settings);
  CHECK(logging_res);
}

constexpr char kSwitchHelp[] = "h";
constexpr char kSwitchSamplers[] = "samplers";
constexpr char kSwitchInitialSample[] = "initial-sample";
constexpr char kSwitchSampleInterval[] = "sample-interval";
constexpr char kSwitchSampleCount[] = "sample-count";
constexpr char kSwitchTimeout[] = "timeout";
constexpr char kSwitchJsonOutputFile[] = "json-output-file";
constexpr char kSwitchSampleOnNotification[] = "sample-on-notification";
constexpr char kSwitchResourceCoalitionPid[] = "resource-coalition-pid";
constexpr char kSwitchSimulateUserActive[] = "simulate-user-active";
constexpr char kSwitchNoSamplers[] = "no-samplers";
constexpr char kUsageString[] = R"(Usage: power_sampler [options]

A tool that samples power-related metrics and states. The tool outputs samples
in CSV or JSON format.

Options:
  --samplers=<samplers>           Comma separated list of samplers.
  --initial-sample                Sample on launch.
  --sample-interval=<num>         Sample on a <num> second interval.
  --sample-on-notification        Sample on power manager notification.
      Sampling on interval or on notification are mutually exclusive.
  --sample-count=<num>            Collect <num> samples before exiting.
  --no-samplers                   Use no samplers.
  --timeout=<num>                 Stops the sampler after <num> seconds.
  --json-output-file=<path>       Produce JSON output to <path> before exit.
      By default output is in CSV format on STDOUT.
  --resource-coalition-pid=<pid>  The pid of a process that is part of a
      resource coalition for which to sample resource usage.
  --simulate-user-active          Simulate user activity periodically, to
                                  perform measurements in the same context as
                                  when the user is active.
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

template <class S>
bool MaybeAddSamplerToController(
    power_sampler::SamplingController& controller) {
  auto sampler = S::Create();
  if (!sampler) {
    std::cerr << "Failed to create requested sampler: " << S::kSamplerName
              << std::endl;
    return false;
  }
  controller.AddSampler(std::move(sampler));
  return true;
}

bool ConsumeSamplerName(const std::string& sampler_name,
                        base::flat_set<std::string>& sampler_names) {
  if (sampler_names.contains(sampler_name)) {
    sampler_names.erase(sampler_name);
    return true;
  }
  return false;
}

std::atomic<bool> should_quit_{false};
void quit_signal_handler(int signal) {
  should_quit_ = true;
}

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

  base::flat_set<std::string> sampler_names;
  if (command_line.HasSwitch(kSwitchSamplers)) {
    std::string samplers_switch =
        command_line.GetSwitchValueASCII(kSwitchSamplers);
    auto sampler_names_vector = SplitString(
        samplers_switch, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& sampler_name : sampler_names_vector) {
      auto result = sampler_names.insert(std::move(sampler_name));
      if (!result.second) {
        PrintUsage("The same sampler was specified twice.");
        return kStatusInvalidParam;
      }
    }
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
    if (command_line.HasSwitch(kSwitchTimeout)) {
      PrintUsage("sample-count should not be specified with --timeout");
      return kStatusInvalidParam;
    }

    std::string sample_count_switch =
        command_line.GetSwitchValueASCII(kSwitchSampleCount);
    if (!base::StringToInt64(sample_count_switch, &sample_count) ||
        sample_count < 1) {
      PrintUsage("sample-count must be numeric and larger than 0.");
      return kStatusInvalidParam;
    }
  }

  base::TimeDelta timeout;
  if (command_line.HasSwitch(kSwitchTimeout)) {
    // Those 2 switches are exclusives but it is already checked when handling
    // --sample-count.
    DCHECK(!command_line.HasSwitch(kSwitchSampleCount));

    std::string timeout_seconds_switch =
        command_line.GetSwitchValueASCII(kSwitchTimeout);
    uint64_t timeout_seconds = 0;
    if (!base::StringToUint64(timeout_seconds_switch, &timeout_seconds) ||
        timeout_seconds < 1) {
      PrintUsage("duration must be numeric and larger than 0.");
      return kStatusInvalidParam;
    }
    timeout = base::Seconds(timeout_seconds);
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

  std::unique_ptr<base::SamplingEventSource> event_source;
  if (command_line.HasSwitch(kSwitchSampleOnNotification)) {
    event_source = std::make_unique<base::IOPMPowerSourceSamplingEventSource>();
  } else {
    event_source =
        std::make_unique<base::TimerSamplingEventSource>(sampling_interval);
  }

  base::SingleThreadTaskExecutor executor(base::MessagePumpType::NS_RUNLOOP);

  power_sampler::SamplingController controller;

  std::unique_ptr<power_sampler::UserActiveSimulator> user_active_simulator;
  if (command_line.HasSwitch(kSwitchSimulateUserActive)) {
    user_active_simulator =
        std::make_unique<power_sampler::UserActiveSimulator>();
    user_active_simulator->Start();
  }

  const base::TimeTicks start_time = base::TimeTicks::Now();

  if (!sampler_names.empty() && command_line.HasSwitch(kSwitchNoSamplers)) {
    PrintUsage("samplers and no-samplers are incompatible");
    return kStatusInvalidParam;
  }

  if (command_line.HasSwitch(kSwitchNoSamplers) &&
      !command_line.HasSwitch(kSwitchSimulateUserActive)) {
    PrintUsage("no samplers and not simulating active user. Nothing to do!");
    return kStatusInvalidParam;
  }

  bool all_samplers =
      sampler_names.empty() && !command_line.HasSwitch(kSwitchNoSamplers);
  if (ConsumeSamplerName(power_sampler::MainDisplaySampler::kSamplerName,
                         sampler_names) ||
      all_samplers) {
    if (!MaybeAddSamplerToController<power_sampler::MainDisplaySampler>(
            controller)) {
      return kStatusRuntimeError;
    }
  }
  if (ConsumeSamplerName(power_sampler::BatterySampler::kSamplerName,
                         sampler_names) ||
      all_samplers) {
    if (!MaybeAddSamplerToController<power_sampler::BatterySampler>(
            controller)) {
      return kStatusRuntimeError;
    }
  }
  if (ConsumeSamplerName(power_sampler::SMCSampler::kSamplerName,
                         sampler_names) ||
      all_samplers) {
    if (!MaybeAddSamplerToController<power_sampler::SMCSampler>(controller)) {
      return kStatusRuntimeError;
    }
  }
  if (ConsumeSamplerName(power_sampler::M1Sampler::kSamplerName,
                         sampler_names) ||
      all_samplers) {
    if (!MaybeAddSamplerToController<power_sampler::M1Sampler>(controller)) {
      return kStatusRuntimeError;
    }
  }
  if (ConsumeSamplerName(power_sampler::UserIdleLevelSampler::kSamplerName,
                         sampler_names) ||
      all_samplers) {
    if (!MaybeAddSamplerToController<power_sampler::UserIdleLevelSampler>(
            controller)) {
      return kStatusRuntimeError;
    }
  }
  if (ConsumeSamplerName(power_sampler::ResourceCoalitionSampler::kSamplerName,
                         sampler_names) ||
      command_line.HasSwitch(kSwitchResourceCoalitionPid)) {
    if (!command_line.HasSwitch(kSwitchResourceCoalitionPid)) {
      PrintUsage(
          "--resource-coalition-pid should be provided to use the resource "
          "coalition sampler.");
    }
    std::string resource_coalition_pid_switch =
        command_line.GetSwitchValueASCII(kSwitchResourceCoalitionPid);
    base::ProcessId pid;
    if (!base::StringToInt(resource_coalition_pid_switch, &pid) || pid < 0) {
      PrintUsage("resource-coalition-pid must be numeric and positive.");
      return kStatusInvalidParam;
    }
    auto sampler =
        power_sampler::ResourceCoalitionSampler::Create(pid, start_time);
    if (!sampler) {
      PrintUsage(
          "Could not create a resource coalition sampler. Is the pid passed to "
          "--resource-coalition-pid valid?");
      return kStatusRuntimeError;
    }
    controller.AddSampler(std::move(sampler));
  }
  // Remaining sampler names are invalid.
  if (!sampler_names.empty()) {
    for (auto sampler_name : sampler_names)
      std::cerr << "Invalid sampler name: " << sampler_name << std::endl;
    return kStatusInvalidParam;
  }

  if (!json_output_file_path.empty()) {
    controller.AddMonitor(power_sampler::JsonExporter::Create(
        std::move(json_output_file_path), start_time));
  } else {
    controller.AddMonitor(power_sampler::CsvExporter::Create(
        start_time, base::File(STDOUT_FILENO)));
  }

  DCHECK(timeout.is_zero() || sample_count == -1);
  if (sample_count > 0) {
    controller.AddMonitor(
        std::make_unique<power_sampler::SampleCounter>(sample_count));
  }

  base::RunLoop run_loop;

  if (!timeout.is_zero()) {
    executor.task_runner()->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                                            timeout);
  }

  // Install signal handler for on-demand quitting. When no samplers are used
  // there is no need to "gracefully quit". In that case no handlers need to be
  // installed.
  if (controller.HasSamplers()) {
    struct sigaction new_action;
    new_action.sa_handler = quit_signal_handler;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_flags = 0;
    sigaction(SIGTERM, &new_action, NULL);
    sigaction(SIGINT, &new_action, NULL);
  }

  base::RepeatingTimer quit_timer;
  quit_timer.Start(
      FROM_HERE, base::Seconds(1),
      BindRepeating(
          [](base::RepeatingTimer* quit_timer) {
            if (should_quit_.load()) {
              std::cerr << "The application is waiting for the last-sample"
                        << std::endl;
              quit_timer->Stop();
            }
          },
          base::Unretained(&quit_timer)));

  auto sample_closure = BindRepeating(
      [](power_sampler::SamplingController* controller,
         base::OnceClosure quit_closure, base::RepeatingTimer* quit_timer) {
        if (controller->OnSamplingEvent() || !quit_timer->IsRunning()) {
          std::move(quit_closure).Run();
        }
      },
      base::Unretained(&controller), run_loop.QuitClosure(),
      base::Unretained(&quit_timer));

  controller.StartSession();

  if (!event_source->Start(sample_closure)) {
    PrintUsage("Could not start the sampling event source.");
    return kStatusRuntimeError;
  }

  if (command_line.HasSwitch(kSwitchInitialSample)) {
    sample_closure.Run();
  }

  run_loop.Run();

  controller.EndSession();

  return kStatusSuccess;
}
