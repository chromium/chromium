// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_executor.h"
#include "base/timer/timer.h"
#include "tools/memory/simulator/memory_simulator.h"
#include "tools/memory/simulator/metrics_printer.h"
#include "tools/memory/simulator/simulator_metrics_provider.h"
#include "tools/memory/simulator/utils.h"

#if BUILDFLAG(IS_MAC)
#include "tools/memory/simulator/process_metrics_provider_mac.h"
#include "tools/memory/simulator/system_metrics_provider_mac.h"
#endif  // BUILDFLAG(IS_MAC)

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
constexpr char kSwitchAllocPerSec[] = "mb_alloc_per_sec";
constexpr char kSwitchReadPerSec[] = "mb_read_per_sec";
constexpr char kSwitchWritePerSec[] = "mb_write_per_sec";
constexpr char kSwitchAllocLimit[] = "alloc_limit";
constexpr char kSwitchStartTimeout[] = "start_timeout";
constexpr char kSwitchReadTimeout[] = "read_timeout";
constexpr char kSwitchWriteTimeout[] = "write_timeout";
constexpr char kSwitchFreeTimeout[] = "free_timeout";
constexpr char kSwitchExitTimeout[] = "exit_timeout";
constexpr char kUsageString[] = R"(Usage: memory_simulator [options]

A tool to allocate, read and write memory pages at a configurable frequency.

Options:
  --mb_alloc_per_sec     Megabytes allocated per second
  --mb_read_per_sec      Megabytes read per second
  --mb_write_per_sec     Megabytes written per second
  --alloc_limit          Stop allocating after this limit (megabytes)
  --start_timeout        Start alloc/read/write after this timeout (seconds)
  --read_timeout         Stop reading after this timeout (seconds)
  --write_timeout        Stop writing reaching this timeout (seconds)
  --free_timeout         Free all memory after this timeout (seconds)
  --exit_timeout        Exit after this timeout (seconds)

All options can be set to inifinite with the value "inf".
)";

void PrintUsageError(const std::string error) {
  std::cerr << "Error: " << error << std::endl << std::endl << kUsageString;
}

// Status code, which can also be used as process exit code. Therefore
// success is explicitly 0.
enum StatusCode {
  kStatusSuccess = 0,
  kStatusUsage = 1,
  kStatusInvalidParam = 2,
  kStatusRuntimeError = 3,
};

std::optional<int64_t> GetInt64Switch(const base::CommandLine& command_line,
                                      const std::string& switch_name,
                                      int64_t default_value) {
  if (!command_line.HasSwitch(switch_name)) {
    return default_value;
  }

  const std::string switch_value =
      command_line.GetSwitchValueASCII(switch_name);
  if (switch_value == "inf") {
    return std::numeric_limits<int64_t>::max();
  }

  int64_t switch_value_int64 = 0;
  if (!base::StringToInt64(switch_value, &switch_value_int64) ||
      switch_value_int64 < 0) {
    PrintUsageError(base::StringPrintf("Switch %s must be a positive integer",
                                       switch_name.c_str()));
    return std::nullopt;
  }

  return switch_value_int64;
}

std::optional<base::TimeDelta> GetSecondsSwitch(
    const base::CommandLine& command_line,
    const std::string& switch_name,
    int64_t default_value) {
  std::optional<int64_t> seconds =
      GetInt64Switch(command_line, switch_name, default_value);
  if (!seconds.has_value()) {
    return std::nullopt;
  }

  if (seconds.value() == std::numeric_limits<int64_t>::max()) {
    return base::TimeDelta::Max();
  }

  return base::Seconds(seconds.value());
}

}  // namespace

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  InitLogging();

#if DCHECK_IS_ON()
  LOG(WARNING) << "!!!!!!";
  LOG(WARNING) << "memory_simulator was built with DCHECKs enabled. This "
                  "changes the behavior of alloc/free and may affect results.";
  LOG(WARNING) << "!!!!!!";
#endif

  if (command_line.HasSwitch(kSwitchHelp)) {
    std::cerr << kUsageString;
    return kStatusUsage;
  }

  std::optional<int64_t> mb_alloc_per_sec =
      GetInt64Switch(command_line, kSwitchAllocPerSec, 512);
  if (!mb_alloc_per_sec.has_value()) {
    return kStatusInvalidParam;
  }

  std::optional<int64_t> mb_read_per_sec =
      GetInt64Switch(command_line, kSwitchReadPerSec, 1024);
  if (!mb_read_per_sec.has_value()) {
    return kStatusInvalidParam;
  }

  std::optional<int64_t> mb_write_per_sec =
      GetInt64Switch(command_line, kSwitchWritePerSec, 1024);
  if (!mb_write_per_sec.has_value()) {
    return kStatusInvalidParam;
  }

  std::optional<int64_t> mb_alloc_limit =
      GetInt64Switch(command_line, kSwitchAllocLimit, 10 * 1024);
  if (!mb_alloc_limit.has_value()) {
    return kStatusInvalidParam;
  }

  std::optional<base::TimeDelta> start_timeout =
      GetSecondsSwitch(command_line, kSwitchStartTimeout, 0);
  if (!start_timeout.has_value()) {
    return kStatusInvalidParam;
  }

  std::optional<base::TimeDelta> read_timeout =
      GetSecondsSwitch(command_line, kSwitchReadTimeout, 5 * 60);
  if (!read_timeout.has_value()) {
    return kStatusInvalidParam;
  }

  std::optional<base::TimeDelta> write_timeout =
      GetSecondsSwitch(command_line, kSwitchWriteTimeout, 5 * 60);
  if (!write_timeout.has_value()) {
    return kStatusInvalidParam;
  }

  std::optional<base::TimeDelta> free_timeout =
      GetSecondsSwitch(command_line, kSwitchFreeTimeout, 10 * 60);
  if (!free_timeout.has_value()) {
    return kStatusInvalidParam;
  }

  std::optional<base::TimeDelta> exit_timeout =
      GetSecondsSwitch(command_line, kSwitchExitTimeout, 5 * 60);
  if (!exit_timeout.has_value()) {
    return kStatusInvalidParam;
  }

  base::SingleThreadTaskExecutor executor;
  base::RunLoop run_loop;

  memory_simulator::MemorySimulator simulator;

  base::OneShotTimer free_timer;
  free_timer.Start(
      FROM_HERE, free_timeout.value(),
      base::BindOnce(&memory_simulator::MemorySimulator::StopAndFree,
                     base::Unretained(&simulator)));

  base::OneShotTimer exit_timer;
  exit_timer.Start(FROM_HERE, exit_timeout.value(), run_loop.QuitClosure());

  memory_simulator::MetricsPrinter printer;
  printer.AddProvider(
      std::make_unique<memory_simulator::SimulatorMetricsProvider>(&simulator));
#if BUILDFLAG(IS_MAC)
  printer.AddProvider(
      std::make_unique<memory_simulator::ProcessMetricsProviderMac>());
  printer.AddProvider(
      std::make_unique<memory_simulator::SystemMetricsProviderMac>());
#endif  // BUILDFLAG(IS_MAC)

  base::RepeatingTimer stats_timer;
  printer.PrintHeader();
  stats_timer.Start(
      FROM_HERE, base::Seconds(5),
      base::BindRepeating(&memory_simulator::MetricsPrinter::PrintStats,
                          base::Unretained(&printer)));

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks read_deadline = now + read_timeout.value();
  base::TimeTicks write_deadline = now + write_timeout.value();

  base::OneShotTimer start_timer;
  start_timer.Start(
      FROM_HERE, start_timeout.value(),
      base::BindOnce(&memory_simulator::MemorySimulator::Start,
                     base::Unretained(&simulator),
                     memory_simulator::MBToPages(mb_alloc_per_sec.value()),
                     memory_simulator::MBToPages(mb_read_per_sec.value()),
                     memory_simulator::MBToPages(mb_write_per_sec.value()),
                     memory_simulator::MBToPages(mb_alloc_limit.value()),
                     read_deadline, write_deadline));

  run_loop.Run();

  return kStatusSuccess;
}
