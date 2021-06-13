// Copyright 2020 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "snapshot/ios/process_snapshot_ios_intermediate_dump.h"

#include <sys/stat.h>

#include "base/logging.h"
#include "snapshot/ios/intermediate_dump_reader_util.h"
#include "util/ios/ios_intermediate_dump_data.h"
#include "util/ios/ios_intermediate_dump_list.h"
#include "util/ios/ios_intermediate_dump_map.h"

namespace {

void MachTimeValueToTimeval(const time_value& mach, timeval* tv) {
  tv->tv_sec = mach.seconds;
  tv->tv_usec = mach.microseconds;
}

}  // namespace

namespace crashpad {
namespace internal {

using Key = internal::IntermediateDumpKey;

bool ProcessSnapshotIOSIntermediateDump::Initialize(
    const base::FilePath& dump_path,
    const std::map<std::string, std::string>& annotations) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  annotations_simple_map_ = annotations;

  if (!reader_.Initialize(dump_path)) {
    return false;
  }

  const IOSIntermediateDumpMap* root_map = reader_.RootMap();
  if (root_map->empty())
    return false;

  uint8_t version;
  if (!GetDataValueFromMap(root_map, Key::kVersion, &version) || version != 1) {
    LOG(ERROR) << "Root map version mismatch";
    return false;
  }

  const internal::IOSIntermediateDumpMap* process_info =
      GetMapFromMap(root_map, Key::kProcessInfo);
  if (!process_info) {
    LOG(ERROR) << "Process snapshot missing required process info map.";
    return false;
  }

  GetDataValueFromMap(process_info, Key::kPID, &p_pid_);
  GetDataValueFromMap(process_info, Key::kParentPID, &e_ppid_);
  GetDataValueFromMap(process_info, Key::kStartTime, &p_starttime_);
  const IOSIntermediateDumpMap* basic_info =
      process_info->GetAsMap(Key::kTaskBasicInfo);
  if (basic_info) {
    GetDataValueFromMap(basic_info, Key::kUserTime, &basic_info_user_time_);
    GetDataValueFromMap(basic_info, Key::kSystemTime, &basic_info_system_time_);
  }

  const IOSIntermediateDumpMap* thread_times =
      process_info->GetAsMap(Key::kTaskThreadTimes);
  if (thread_times) {
    GetDataValueFromMap(basic_info, Key::kUserTime, &thread_times_user_time_);
    GetDataValueFromMap(
        basic_info, Key::kSystemTime, &thread_times_system_time_);
  }

  GetDataValueFromMap(process_info, Key::kSnapshotTime, &snapshot_time_);

  const IOSIntermediateDumpMap* system_info =
      GetMapFromMap(root_map, Key::kSystemInfo);
  if (!system_info) {
    LOG(ERROR) << "Process snapshot missing required system info map.";
    return false;
  }
  system_.Initialize(system_info);

  // Threads
  const IOSIntermediateDumpList* thread_list =
      GetListFromMap(root_map, Key::kThreads);
  if (thread_list) {
    for (const auto& value : *thread_list) {
      auto thread =
          std::make_unique<internal::ThreadSnapshotIOSIntermediateDump>();
      if (thread->Initialize(value.get())) {
        threads_.push_back(std::move(thread));
      }
    }
  }

  const IOSIntermediateDumpList* module_list =
      GetListFromMap(root_map, Key::kModules);
  if (module_list) {
    for (const auto& value : *module_list) {
      auto module =
          std::make_unique<internal::ModuleSnapshotIOSIntermediateDump>();
      if (module->Initialize(value.get())) {
        modules_.push_back(std::move(module));
      }
    }
  }

  // Exceptions
  const IOSIntermediateDumpMap* signal_exception =
      root_map->GetAsMap(Key::kSignalException);
  const IOSIntermediateDumpMap* mach_exception =
      root_map->GetAsMap(Key::kMachException);
  const IOSIntermediateDumpMap* ns_exception =
      root_map->GetAsMap(Key::kNSException);
  if (signal_exception) {
    exception_.reset(new internal::ExceptionSnapshotIOSIntermediateDump());
    if (!exception_->InitializeFromSignal(signal_exception)) {
      LOG(ERROR) << "Process snapshot could not initialize signal exception.";
      return false;
    }
  } else if (mach_exception) {
    exception_.reset(new internal::ExceptionSnapshotIOSIntermediateDump());
    if (!exception_->InitializeFromMachException(
            mach_exception, GetListFromMap(root_map, Key::kThreads))) {
      LOG(ERROR) << "Process snapshot could not initialize Mach exception.";
      return false;
    }
  } else if (ns_exception) {
    exception_.reset(new internal::ExceptionSnapshotIOSIntermediateDump());
    if (!exception_->InitializeFromNSException(
            ns_exception, GetListFromMap(root_map, Key::kThreads))) {
      LOG(ERROR) << "Process snapshot could not initialize NSException.";
      return false;
    }
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

pid_t ProcessSnapshotIOSIntermediateDump::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return p_pid_;
}

pid_t ProcessSnapshotIOSIntermediateDump::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return e_ppid_;
}

void ProcessSnapshotIOSIntermediateDump::SnapshotTime(
    timeval* snapshot_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *snapshot_time = snapshot_time_;
}

void ProcessSnapshotIOSIntermediateDump::ProcessStartTime(
    timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *start_time = p_starttime_;
}

void ProcessSnapshotIOSIntermediateDump::ProcessCPUTimes(
    timeval* user_time,
    timeval* system_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  // Calculate user and system time the same way the kernel does for
  // getrusage(). See 10.15.0 xnu-6153.11.26/bsd/kern/kern_resource.c calcru().
  timerclear(user_time);
  timerclear(system_time);

  MachTimeValueToTimeval(basic_info_user_time_, user_time);
  MachTimeValueToTimeval(basic_info_system_time_, system_time);

  timeval thread_user_time;
  MachTimeValueToTimeval(thread_times_user_time_, &thread_user_time);
  timeval thread_system_time;
  MachTimeValueToTimeval(thread_times_system_time_, &thread_system_time);

  timeradd(user_time, &thread_user_time, user_time);
  timeradd(system_time, &thread_system_time, system_time);
}

void ProcessSnapshotIOSIntermediateDump::ReportID(UUID* report_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *report_id = report_id_;
}

void ProcessSnapshotIOSIntermediateDump::ClientID(UUID* client_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *client_id = client_id_;
}

const std::map<std::string, std::string>&
ProcessSnapshotIOSIntermediateDump::AnnotationsSimpleMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_simple_map_;
}

const SystemSnapshot* ProcessSnapshotIOSIntermediateDump::System() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &system_;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotIOSIntermediateDump::Threads()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ThreadSnapshot*> threads;
  for (const auto& thread : threads_) {
    threads.push_back(thread.get());
  }
  return threads;
}

std::vector<const ModuleSnapshot*> ProcessSnapshotIOSIntermediateDump::Modules()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ModuleSnapshot*> modules;
  for (const auto& module : modules_) {
    modules.push_back(module.get());
  }
  return modules;
}

std::vector<UnloadedModuleSnapshot>
ProcessSnapshotIOSIntermediateDump::UnloadedModules() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<UnloadedModuleSnapshot>();
}

const ExceptionSnapshot* ProcessSnapshotIOSIntermediateDump::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_.get();
}

std::vector<const MemoryMapRegionSnapshot*>
ProcessSnapshotIOSIntermediateDump::MemoryMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemoryMapRegionSnapshot*>();
}

std::vector<HandleSnapshot> ProcessSnapshotIOSIntermediateDump::Handles()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<HandleSnapshot>();
}

std::vector<const MemorySnapshot*>
ProcessSnapshotIOSIntermediateDump::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemorySnapshot*>();
}

const ProcessMemory* ProcessSnapshotIOSIntermediateDump::Memory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return nullptr;
}

}  // namespace internal
}  // namespace crashpad
