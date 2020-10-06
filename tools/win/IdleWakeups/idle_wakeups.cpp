// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include <algorithm>
#include <map>
#include <vector>

#include "power_sampler.h"
#include "system_information_sampler.h"

// Unit for raw CPU usage data from Windows.
constexpr int kTicksPerSecond = 10000000;

// Result data structure contains a final set of values calculated based on
// comparison of two snapshots. These are the values that the tool prints
// in the output.
struct Result {
  ULONG idle_wakeups_per_sec;
  double cpu_usage_percent;
  double cpu_usage_seconds;
  ULONGLONG working_set;
  double power;
};

typedef std::vector<Result> ResultVector;

// The following 4 functions are used for sorting of ResultVector.
ULONG GetIdleWakeupsPerSec(const Result& r) {
  return r.idle_wakeups_per_sec;
}
double GetCpuUsagePercent(const Result& r) {
  return r.cpu_usage_percent;
}
double GetCpuUsageSeconds(const Result& r) {
  return r.cpu_usage_seconds;
}
ULONGLONG GetWorkingSet(const Result& r) {
  return r.working_set;
}
double GetPower(const Result& r) {
  return r.power;
}

template <typename T>
T GetMedian(ResultVector* results, T (*getter)(const Result&)) {
  std::sort(results->begin(), results->end(),
            [&](const Result& lhs, const Result& rhs) {
              return getter(lhs) < getter(rhs);
            });

  size_t median_index = results->size() / 2;
  if (results->size() % 2 != 0) {
    return getter((*results)[median_index]);
  } else {
    return (getter((*results)[median_index - 1]) +
            getter((*results)[median_index])) /
           2;
  }
}

// Count newly created processes: those in |processes| but not
// |previous_processes|.
size_t GetNumProcessesCreated(const ProcessDataMap& previous_processes,
                              const ProcessDataMap& processes) {
  size_t num_processes_created = 0;
  for (auto& process : processes) {
    if (previous_processes.find(process.first) == previous_processes.end())
      num_processes_created++;
  }
  return num_processes_created;
}

// This class holds the app state and contains a number of utilities for
// collecting and diffing snapshots of data, handling processes, etc.
class IdleWakeups {
 public:
  IdleWakeups();
  ~IdleWakeups();

  Result DiffSnapshots(const ProcessDataSnapshot& prev_snapshot,
                       const ProcessDataSnapshot& snapshot);

  void OpenProcesses(const ProcessDataSnapshot& snapshot);
  void CloseProcesses();

 private:
  HANDLE GetProcessHandle(ProcessId process_id);
  void OpenProcess(ProcessId process_id);
  void CloseProcess(ProcessId process_id);
  bool GetFinishedProcessCpuTime(ProcessId process_id, ULONGLONG* cpu_usage);

  static ULONG CountContextSwitches(const ProcessData& process_data);
  static ULONG DiffContextSwitches(const ProcessData& prev_process_data,
                                   const ProcessData& process_data);

  std::map<ProcessId, HANDLE> process_id_to_handle_map;

  IdleWakeups& operator=(const IdleWakeups&) = delete;
  IdleWakeups(const IdleWakeups&) = delete;
};

IdleWakeups::IdleWakeups() {}

IdleWakeups::~IdleWakeups() {
  CloseProcesses();
}

void IdleWakeups::OpenProcesses(const ProcessDataSnapshot& snapshot) {
  for (auto& pair : snapshot.processes) {
    OpenProcess(pair.first);
  }
}

void IdleWakeups::CloseProcesses() {
  for (auto& pair : process_id_to_handle_map) {
    CloseHandle(pair.second);
  }
  process_id_to_handle_map.clear();
}

HANDLE IdleWakeups::GetProcessHandle(ProcessId process_id) {
  return process_id_to_handle_map[process_id];
}

void IdleWakeups::OpenProcess(ProcessId process_id) {
  process_id_to_handle_map[process_id] = ::OpenProcess(
      PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)(ULONGLONG)process_id);
}

void IdleWakeups::CloseProcess(ProcessId process_id) {
  HANDLE handle = GetProcessHandle(process_id);
  CloseHandle(handle);
  process_id_to_handle_map.erase(process_id);
}

ULONG IdleWakeups::CountContextSwitches(const ProcessData& process_data) {
  ULONG context_switches = 0;

  for (const auto& thread_data : process_data.threads) {
    context_switches += thread_data.context_switches;
  }

  return context_switches;
}

ULONG IdleWakeups::DiffContextSwitches(const ProcessData& prev_process_data,
                                       const ProcessData& process_data) {
  ULONG context_switches = 0;
  size_t prev_index = 0;

  for (const auto& thread_data : process_data.threads) {
    ULONG prev_context_switches = 0;

    for (; prev_index < prev_process_data.threads.size(); ++prev_index) {
      const auto& prev_thread_data = prev_process_data.threads[prev_index];
      if (prev_thread_data.thread_id == thread_data.thread_id) {
        prev_context_switches = prev_thread_data.context_switches;
        ++prev_index;
        break;
      }

      if (prev_thread_data.thread_id > thread_data.thread_id)
        break;
    }

    context_switches += thread_data.context_switches - prev_context_switches;
  }

  return context_switches;
}

bool IdleWakeups::GetFinishedProcessCpuTime(ProcessId process_id,
                                            ULONGLONG* cpu_time) {
  HANDLE process_handle = GetProcessHandle(process_id);

  FILETIME creation_time, exit_time, kernel_time, user_time;
  if (GetProcessTimes(process_handle, &creation_time, &exit_time, &kernel_time,
                      &user_time)) {
    ULARGE_INTEGER ul_kernel_time, ul_user_time;
    ul_kernel_time.LowPart = kernel_time.dwLowDateTime;
    ul_kernel_time.HighPart = kernel_time.dwHighDateTime;
    ul_user_time.LowPart = user_time.dwLowDateTime;
    ul_user_time.HighPart = user_time.dwHighDateTime;
    *cpu_time = ul_kernel_time.QuadPart + ul_user_time.QuadPart;
    return true;
  }

  *cpu_time = 0;
  return false;
}

Result IdleWakeups::DiffSnapshots(const ProcessDataSnapshot& prev_snapshot,
                                  const ProcessDataSnapshot& snapshot) {
  ULONG idle_wakeups_delta = 0;
  ULONGLONG cpu_usage_delta = 0;
  ULONGLONG total_working_set = 0;

  ProcessDataMap::const_iterator prev_it = prev_snapshot.processes.begin();

  for (const auto& it : snapshot.processes) {
    ProcessId process_id = it.first;
    const ProcessData& process_data = it.second;
    const ProcessData* prev_process_data_to_diff = nullptr;
    ULONGLONG prev_process_cpu_time = 0;

    for (; prev_it != prev_snapshot.processes.end(); ++prev_it) {
      ProcessId prev_process_id = prev_it->first;
      const ProcessData& prev_process_data = prev_it->second;

      if (prev_process_id == process_id) {
        prev_process_data_to_diff = &prev_process_data;
        prev_process_cpu_time = prev_process_data.cpu_time;
        ++prev_it;
        break;
      }

      if (prev_process_id > process_id)
        break;

      // Prev process disappeared.
      ULONGLONG last_known_cpu_time;
      if (GetFinishedProcessCpuTime(prev_process_id, &last_known_cpu_time)) {
        cpu_usage_delta += last_known_cpu_time - prev_process_data.cpu_time;
      }
      CloseProcess(prev_process_id);
    }

    if (prev_process_data_to_diff) {
      idle_wakeups_delta +=
          DiffContextSwitches(*prev_process_data_to_diff, process_data);
    } else {
      // New process that we haven't seen before.
      OpenProcess(process_id);
      idle_wakeups_delta += CountContextSwitches(process_data);
    }

    cpu_usage_delta += process_data.cpu_time - prev_process_cpu_time;
    total_working_set += process_data.working_set / 1024;
  }

  double time_delta = snapshot.timestamp - prev_snapshot.timestamp;
  Result result;
  result.idle_wakeups_per_sec =
      static_cast<ULONG>(idle_wakeups_delta / time_delta);
  result.cpu_usage_percent =
      (double)cpu_usage_delta * 100 / (time_delta * kTicksPerSecond);
  result.cpu_usage_seconds = (double)cpu_usage_delta / kTicksPerSecond;
  result.working_set = total_working_set;

  return result;
}

HANDLE ctrl_c_pressed = NULL;

BOOL WINAPI HandlerFunction(DWORD ctrl_type) {
  if (ctrl_type == CTRL_C_EVENT) {
    printf("Ctrl+C pressed...\n");
    SetEvent(ctrl_c_pressed);
    return TRUE;
  }

  return FALSE;
}

const DWORD sleep_time_sec = 2;

void PrintHeader() {
  printf(
      "------------------------------------------------------------------------"
      "----------\n");
  printf(
      "                                                            Private\n"
      "                       Context switches/sec    CPU usage    Working set "
      "     Power\n");
  printf(
      "------------------------------------------------------------------------"
      "----------\n");
}

#define RESULT_FORMAT_STRING "    %20lu    %8.2f%c    %7.2f MiB    %5.2f W\n"

int wmain(int argc, wchar_t* argv[]) {
  ctrl_c_pressed = CreateEvent(NULL, FALSE, FALSE, NULL);
  SetConsoleCtrlHandler(HandlerFunction, TRUE);

  PowerSampler power_sampler;
  IdleWakeups the_app;

  // Parse command line for target process name and optional --cpu-seconds flag.
  wchar_t* target_process_name = nullptr;
  bool cpu_usage_in_seconds = false;
  for (int i = 1; i < argc; i++) {
    if (wcscmp(argv[i], L"--cpu-seconds") == 0)
      cpu_usage_in_seconds = true;
    else if (!target_process_name)
      target_process_name = argv[i];

    // Stop parsing if all possible args have been found.
    if (cpu_usage_in_seconds && target_process_name)
      break;
  }
  const char cpu_usage_unit = cpu_usage_in_seconds ? 's' : '%';
  SystemInformationSampler system_information_sampler(
      target_process_name ? target_process_name : L"chrome.exe");

  // Take the initial snapshot.
  std::unique_ptr<ProcessDataSnapshot> previous_snapshot =
      system_information_sampler.TakeSnapshot();

  the_app.OpenProcesses(*previous_snapshot);
  const size_t initial_number_of_processes =
      previous_snapshot->processes.size();
  size_t final_number_of_processes = initial_number_of_processes;

  ULONG cumulative_idle_wakeups_per_sec = 0;
  double cumulative_cpu_usage_percent = 0.0;
  double cumulative_cpu_usage_seconds = 0.0;
  ULONGLONG cumulative_working_set = 0;
  double cumulative_energy = 0.0;
  size_t cumulative_processes_created = 0;
  int num_idle_snapshots = 0;

  ResultVector results;

  printf("Capturing perf data for all processes matching %ls\n",
         system_information_sampler.target_process_name_filter());

  PrintHeader();

  for (;;) {
    if (WaitForSingleObject(ctrl_c_pressed, sleep_time_sec * 1000) ==
        WAIT_OBJECT_0)
      break;

    std::unique_ptr<ProcessDataSnapshot> snapshot =
        system_information_sampler.TakeSnapshot();
    size_t number_of_processes = snapshot->processes.size();
    final_number_of_processes = number_of_processes;

    cumulative_processes_created += GetNumProcessesCreated(
        previous_snapshot->processes, snapshot->processes);

    Result result = the_app.DiffSnapshots(*previous_snapshot, *snapshot);
    previous_snapshot = std::move(snapshot);

    power_sampler.SampleCPUPowerState();
    result.power = power_sampler.get_power(L"Processor");

    printf("%9u processes" RESULT_FORMAT_STRING, (DWORD)number_of_processes,
           result.idle_wakeups_per_sec,
           cpu_usage_in_seconds ? result.cpu_usage_seconds
                                : result.cpu_usage_percent,
           cpu_usage_unit, result.working_set / 1024.0, result.power);

    if (number_of_processes > 0) {
      cumulative_idle_wakeups_per_sec += result.idle_wakeups_per_sec;
      cumulative_cpu_usage_percent += result.cpu_usage_percent;
      cumulative_cpu_usage_seconds += result.cpu_usage_seconds;
      cumulative_working_set += result.working_set;
      cumulative_energy += result.power;
      results.push_back(result);
    } else {
      num_idle_snapshots++;
    }
  }

  CloseHandle(ctrl_c_pressed);

  ULONG sample_count = (ULONG)results.size();
  if (sample_count == 0)
    return 0;

  PrintHeader();

  printf("            Average" RESULT_FORMAT_STRING,
         cumulative_idle_wakeups_per_sec / sample_count,
         (cpu_usage_in_seconds ? cumulative_cpu_usage_seconds
                               : cumulative_cpu_usage_percent) /
             sample_count,
         cpu_usage_unit, (cumulative_working_set / 1024.0) / sample_count,
         cumulative_energy / sample_count);

  Result median_result;

  median_result.idle_wakeups_per_sec =
      GetMedian<ULONG>(&results, GetIdleWakeupsPerSec);
  median_result.cpu_usage_percent =
      GetMedian<double>(&results, GetCpuUsagePercent);
  median_result.cpu_usage_seconds =
      GetMedian<double>(&results, GetCpuUsageSeconds);
  median_result.working_set = GetMedian<ULONGLONG>(&results, GetWorkingSet);
  median_result.power = GetMedian<double>(&results, GetPower);

  printf("             Median" RESULT_FORMAT_STRING,
         median_result.idle_wakeups_per_sec,
         cpu_usage_in_seconds ? median_result.cpu_usage_seconds
                              : median_result.cpu_usage_percent,
         cpu_usage_unit, median_result.working_set / 1024.0,
         median_result.power);

  if (cpu_usage_in_seconds) {
    printf("                Sum    %32.2f%c\n", cumulative_cpu_usage_seconds,
           cpu_usage_unit);
  }

  printf("\n");
  if (num_idle_snapshots > 0)
    printf("Idle snapshots:      %d\n", num_idle_snapshots);
  printf("Processes created:   %zu\n", cumulative_processes_created);
  printf("Processes destroyed: %zu\n", initial_number_of_processes +
                                           cumulative_processes_created -
                                           final_number_of_processes);

  return 0;
}
