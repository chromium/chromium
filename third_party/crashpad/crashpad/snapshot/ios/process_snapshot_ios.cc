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

#include "snapshot/ios/process_snapshot_ios.h"

#include <mach-o/loader.h>
#include <mach/mach.h>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/stl_util.h"

namespace {

void MachTimeValueToTimeval(const time_value& mach, timeval* tv) {
  tv->tv_sec = mach.seconds;
  tv->tv_usec = mach.microseconds;
}

}  // namespace

namespace crashpad {

ProcessSnapshotIOS::ProcessSnapshotIOS()
    : ProcessSnapshot(),
      kern_proc_info_(),
      basic_info_user_time_(),
      basic_info_system_time_(),
      thread_times_user_time_(),
      thread_times_system_time_(),
      system_(),
      threads_(),
      modules_(),
      exception_(),
      report_id_(),
      client_id_(),
      annotations_simple_map_(),
      snapshot_time_(),
      initialized_() {}

ProcessSnapshotIOS::~ProcessSnapshotIOS() {}

bool ProcessSnapshotIOS::Initialize(
    const internal::IOSSystemDataCollector& system_data) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  // Used by pid, parent pid and snapshot time.
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  size_t len = sizeof(kern_proc_info_);
  if (sysctl(mib, base::size(mib), &kern_proc_info_, &len, nullptr, 0)) {
    PLOG(ERROR) << "sysctl";
    return false;
  }

  // Used by user time and system time.
  task_basic_info_64 task_basic_info;
  mach_msg_type_number_t task_basic_info_count = TASK_BASIC_INFO_64_COUNT;
  kern_return_t kr = task_info(mach_task_self(),
                               TASK_BASIC_INFO_64,
                               reinterpret_cast<task_info_t>(&task_basic_info),
                               &task_basic_info_count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(WARNING, kr) << "task_info TASK_BASIC_INFO_64";
    return false;
  }

  task_thread_times_info_data_t task_thread_times;
  mach_msg_type_number_t task_thread_times_count = TASK_THREAD_TIMES_INFO_COUNT;
  kr = task_info(mach_task_self(),
                 TASK_THREAD_TIMES_INFO,
                 reinterpret_cast<task_info_t>(&task_thread_times),
                 &task_thread_times_count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(WARNING, kr) << "task_info TASK_THREAD_TIMES";
  }

  basic_info_user_time_ = task_basic_info.user_time;
  basic_info_system_time_ = task_basic_info.system_time;
  thread_times_user_time_ = task_thread_times.user_time;
  thread_times_system_time_ = task_thread_times.system_time;

  if (gettimeofday(&snapshot_time_, nullptr) != 0) {
    PLOG(ERROR) << "gettimeofday";
    return false;
  }

  system_.Initialize(system_data);
  InitializeThreads();
  InitializeModules();

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

void ProcessSnapshotIOS::SetExceptionFromSignal(const siginfo_t* siginfo,
                                                const ucontext_t* context) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  DCHECK(!exception_.get());

  exception_.reset(new internal::ExceptionSnapshotIOS());
  exception_->InitializeFromSignal(siginfo, context);
}

void ProcessSnapshotIOS::SetExceptionFromMachException(
    exception_behavior_t behavior,
    thread_t exception_thread,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t flavor,
    ConstThreadState old_state,
    mach_msg_type_number_t old_state_count) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  DCHECK(!exception_.get());

  exception_.reset(new internal::ExceptionSnapshotIOS());
  exception_->InitializeFromMachException(behavior,
                                          exception_thread,
                                          exception,
                                          code,
                                          code_count,
                                          flavor,
                                          old_state,
                                          old_state_count);
}

pid_t ProcessSnapshotIOS::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return kern_proc_info_.kp_proc.p_pid;
}

pid_t ProcessSnapshotIOS::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return kern_proc_info_.kp_eproc.e_ppid;
}

void ProcessSnapshotIOS::SnapshotTime(timeval* snapshot_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *snapshot_time = snapshot_time_;
}

void ProcessSnapshotIOS::ProcessStartTime(timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *start_time = kern_proc_info_.kp_proc.p_starttime;
}

void ProcessSnapshotIOS::ProcessCPUTimes(timeval* user_time,
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

void ProcessSnapshotIOS::ReportID(UUID* report_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *report_id = report_id_;
}

void ProcessSnapshotIOS::ClientID(UUID* client_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *client_id = client_id_;
}

const std::map<std::string, std::string>&
ProcessSnapshotIOS::AnnotationsSimpleMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_simple_map_;
}

const SystemSnapshot* ProcessSnapshotIOS::System() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &system_;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotIOS::Threads() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ThreadSnapshot*> threads;
  for (const auto& thread : threads_) {
    threads.push_back(thread.get());
  }
  return threads;
}

std::vector<const ModuleSnapshot*> ProcessSnapshotIOS::Modules() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ModuleSnapshot*> modules;
  for (const auto& module : modules_) {
    modules.push_back(module.get());
  }
  return modules;
}

std::vector<UnloadedModuleSnapshot> ProcessSnapshotIOS::UnloadedModules()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<UnloadedModuleSnapshot>();
}

const ExceptionSnapshot* ProcessSnapshotIOS::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_.get();
}

std::vector<const MemoryMapRegionSnapshot*> ProcessSnapshotIOS::MemoryMap()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemoryMapRegionSnapshot*>();
}

std::vector<HandleSnapshot> ProcessSnapshotIOS::Handles() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<HandleSnapshot>();
}

std::vector<const MemorySnapshot*> ProcessSnapshotIOS::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemorySnapshot*>();
}

const ProcessMemory* ProcessSnapshotIOS::Memory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return nullptr;
}

void ProcessSnapshotIOS::InitializeThreads() {
  mach_msg_type_number_t thread_count = 0;
  const thread_act_array_t threads =
      internal::ThreadSnapshotIOS::GetThreads(&thread_count);
  for (uint32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
    thread_t thread = threads[thread_index];
    auto thread_snapshot = std::make_unique<internal::ThreadSnapshotIOS>();
    if (thread_snapshot->Initialize(thread)) {
      threads_.push_back(std::move(thread_snapshot));
    }
    mach_port_deallocate(mach_task_self(), thread);
  }
  // TODO(justincohen): This dealloc above and below needs to move with the
  // call to task_threads inside internal::ThreadSnapshotIOS::GetThreads.
  vm_deallocate(mach_task_self(),
                reinterpret_cast<vm_address_t>(threads),
                sizeof(thread_t) * thread_count);
}

void ProcessSnapshotIOS::InitializeModules() {
  const dyld_all_image_infos* image_infos =
      internal::ModuleSnapshotIOS::DyldAllImageInfo();

  uint32_t image_count = image_infos->infoArrayCount;
  const dyld_image_info* image_array = image_infos->infoArray;
  for (uint32_t image_index = 0; image_index < image_count; ++image_index) {
    const dyld_image_info* image = &image_array[image_index];
    auto module = std::make_unique<internal::ModuleSnapshotIOS>();
    if (module->Initialize(image)) {
      modules_.push_back(std::move(module));
    }
  }
  auto module = std::make_unique<internal::ModuleSnapshotIOS>();
  if (module->InitializeDyld(image_infos)) {
    modules_.push_back(std::move(module));
  }
}

}  // namespace crashpad
