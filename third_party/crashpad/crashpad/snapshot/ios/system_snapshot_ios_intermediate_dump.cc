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

#include "snapshot/ios/system_snapshot_ios_intermediate_dump.h"

#include <mach/mach.h>
#include <stddef.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <algorithm>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "snapshot/cpu_context.h"
#include "snapshot/ios/intermediate_dump_reader_util.h"
#include "snapshot/posix/timezone.h"
#include "util/ios/ios_intermediate_dump_data.h"
#include "util/mac/mac_util.h"
#include "util/numeric/in_range_cast.h"

namespace crashpad {

namespace internal {

using Key = IntermediateDumpKey;

SystemSnapshotIOSIntermediateDump::SystemSnapshotIOSIntermediateDump()
    : SystemSnapshot(),
      os_version_build_(),
      machine_description_(),
      os_version_major_(0),
      os_version_minor_(0),
      os_version_bugfix_(0),
      active_(0),
      inactive_(0),
      wired_(0),
      free_(0),
      cpu_count_(0),
      cpu_vendor_(),
      dst_status_(),
      standard_offset_seconds_(0),
      daylight_offset_seconds_(0),
      standard_name_(),
      daylight_name_(),
      initialized_() {}

SystemSnapshotIOSIntermediateDump::~SystemSnapshotIOSIntermediateDump() {}

void SystemSnapshotIOSIntermediateDump::Initialize(
    const IOSIntermediateDumpMap* system_data) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  GetDataStringFromMap(system_data, Key::kOSVersionBuild, &os_version_build_);
  GetDataStringFromMap(
      system_data, Key::kMachineDescription, &machine_description_);
  GetDataStringFromMap(system_data, Key::kCpuVendor, &cpu_vendor_);
  GetDataStringFromMap(system_data, Key::kStandardName, &standard_name_);
  GetDataStringFromMap(system_data, Key::kDaylightName, &daylight_name_);

  GetDataValueFromMap(system_data, Key::kOSVersionMajor, &os_version_major_);
  GetDataValueFromMap(system_data, Key::kOSVersionMinor, &os_version_minor_);
  GetDataValueFromMap(system_data, Key::kOSVersionBugfix, &os_version_bugfix_);
  GetDataValueFromMap(system_data, Key::kCpuCount, &cpu_count_);

  GetDataValueFromMap(
      system_data, Key::kStandardOffsetSeconds, &standard_offset_seconds_);
  GetDataValueFromMap(
      system_data, Key::kDaylightOffsetSeconds, &daylight_offset_seconds_);

  bool has_daylight_saving_time;
  GetDataValueFromMap(
      system_data, Key::kHasDaylightSavingTime, &has_daylight_saving_time);
  bool is_daylight_saving_time;
  GetDataValueFromMap(
      system_data, Key::kIsDaylightSavingTime, &is_daylight_saving_time);

  if (has_daylight_saving_time) {
    dst_status_ = is_daylight_saving_time
                      ? SystemSnapshot::kObservingDaylightSavingTime
                      : SystemSnapshot::kObservingStandardTime;
  } else {
    dst_status_ = SystemSnapshot::kDoesNotObserveDaylightSavingTime;
  }

  vm_size_t page_size;
  if (GetDataValueFromMap(system_data, Key::kPageSize, &page_size)) {
    const IOSIntermediateDumpMap* vm_stat =
        GetMapFromMap(system_data, Key::kVMStat);
    if (vm_stat) {
      GetDataValueFromMap(vm_stat, Key::kActive, &active_);
      active_ *= page_size;

      GetDataValueFromMap(vm_stat, Key::kInactive, &inactive_);
      inactive_ *= page_size;

      GetDataValueFromMap(vm_stat, Key::kWired, &wired_);
      wired_ *= page_size;

      GetDataValueFromMap(vm_stat, Key::kFree, &free_);
      free_ *= page_size;
    }
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
}

CPUArchitecture SystemSnapshotIOSIntermediateDump::GetCPUArchitecture() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
#if defined(ARCH_CPU_X86_64)
  return kCPUArchitectureX86_64;
#elif defined(ARCH_CPU_ARM64)
  return kCPUArchitectureARM64;
#endif
}

uint32_t SystemSnapshotIOSIntermediateDump::CPURevision() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): sysctlbyname machdep.cpu.* returns -1 on iOS/ARM64, but
  // consider recording this for X86_64 only.
  return 0;
}

uint8_t SystemSnapshotIOSIntermediateDump::CPUCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_count_;
}

std::string SystemSnapshotIOSIntermediateDump::CPUVendor() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_vendor_;
}

void SystemSnapshotIOSIntermediateDump::CPUFrequency(uint64_t* current_hz,
                                                     uint64_t* max_hz) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): sysctlbyname hw.cpufrequency returns -1 on iOS/ARM64,
  // but consider recording this for X86_64 only.
  *current_hz = 0;
  *max_hz = 0;
}

uint32_t SystemSnapshotIOSIntermediateDump::CPUX86Signature() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return 0;
}

uint64_t SystemSnapshotIOSIntermediateDump::CPUX86Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return 0;
}

uint64_t SystemSnapshotIOSIntermediateDump::CPUX86ExtendedFeatures() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return 0;
}

uint32_t SystemSnapshotIOSIntermediateDump::CPUX86Leaf7Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return 0;
}

bool SystemSnapshotIOSIntermediateDump::CPUX86SupportsDAZ() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return false;
}

SystemSnapshot::OperatingSystem
SystemSnapshotIOSIntermediateDump::GetOperatingSystem() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return kOperatingSystemIOS;
}

bool SystemSnapshotIOSIntermediateDump::OSServer() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return false;
}

void SystemSnapshotIOSIntermediateDump::OSVersion(int* major,
                                                  int* minor,
                                                  int* bugfix,
                                                  std::string* build) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *major = os_version_major_;
  *minor = os_version_minor_;
  *bugfix = os_version_bugfix_;
  build->assign(os_version_build_);
}

std::string SystemSnapshotIOSIntermediateDump::OSVersionFull() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return base::StringPrintf("%d.%d.%d %s",
                            os_version_major_,
                            os_version_minor_,
                            os_version_bugfix_,
                            os_version_build_.c_str());
}

std::string SystemSnapshotIOSIntermediateDump::MachineDescription() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return machine_description_;
}

bool SystemSnapshotIOSIntermediateDump::NXEnabled() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider using kern.nx when available (pre-iOS 13,
  // pre-OS X 10.15). Otherwise the bit is always enabled.
  return true;
}

void SystemSnapshotIOSIntermediateDump::TimeZone(
    DaylightSavingTimeStatus* dst_status,
    int* standard_offset_seconds,
    int* daylight_offset_seconds,
    std::string* standard_name,
    std::string* daylight_name) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *dst_status = dst_status_;
  *standard_offset_seconds = standard_offset_seconds_;
  *daylight_offset_seconds = daylight_offset_seconds_;
  standard_name->assign(standard_name_);
  daylight_name->assign(daylight_name_);
}

}  // namespace internal
}  // namespace crashpad
