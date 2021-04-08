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

#include "snapshot/ios/system_snapshot_ios.h"

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
#include "snapshot/posix/timezone.h"
#include "util/mac/mac_util.h"
#include "util/numeric/in_range_cast.h"

namespace crashpad {

namespace internal {

SystemSnapshotIOS::SystemSnapshotIOS()
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

SystemSnapshotIOS::~SystemSnapshotIOS() {}

void SystemSnapshotIOS::Initialize(const IOSSystemDataCollector& system_data) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  system_data.OSVersion(
      &os_version_major_, &os_version_minor_, &os_version_bugfix_);
  os_version_build_ = system_data.Build();
  machine_description_ = system_data.MachineDescription();
  cpu_count_ = system_data.ProcessorCount();
  cpu_vendor_ = system_data.CPUVendor();
  if (system_data.HasDaylightSavingTime()) {
    dst_status_ = system_data.IsDaylightSavingTime()
                      ? SystemSnapshot::kObservingDaylightSavingTime
                      : SystemSnapshot::kObservingStandardTime;
  } else {
    dst_status_ = SystemSnapshot::kDoesNotObserveDaylightSavingTime;
  }
  standard_offset_seconds_ = system_data.StandardOffsetSeconds();
  daylight_offset_seconds_ = system_data.DaylightOffsetSeconds();
  standard_name_ = system_data.StandardName();
  daylight_name_ = system_data.DaylightName();

  // Currently unused by minidump.
  vm_size_t page_size;
  host_page_size(mach_host_self(), &page_size);
  mach_msg_type_number_t host_size =
      sizeof(vm_statistics_data_t) / sizeof(integer_t);
  vm_statistics_data_t vm_stat;
  kern_return_t kr = host_statistics(mach_host_self(),
                                     HOST_VM_INFO,
                                     reinterpret_cast<host_info_t>(&vm_stat),
                                     &host_size);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(WARNING, kr) << "host_statistics";
  }
  active_ = vm_stat.active_count * page_size;
  inactive_ = vm_stat.inactive_count * page_size;
  wired_ = vm_stat.wire_count * page_size;
  free_ = vm_stat.free_count * page_size;

  INITIALIZATION_STATE_SET_VALID(initialized_);
}

CPUArchitecture SystemSnapshotIOS::GetCPUArchitecture() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
#if defined(ARCH_CPU_X86_64)
  return kCPUArchitectureX86_64;
#elif defined(ARCH_CPU_ARM64)
  return kCPUArchitectureARM64;
#endif
}

uint32_t SystemSnapshotIOS::CPURevision() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): sysctlbyname machdep.cpu.* returns -1 on iOS/ARM64, but
  // consider recording this for X86_64 only.
  return 0;
}

uint8_t SystemSnapshotIOS::CPUCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_count_;
}

std::string SystemSnapshotIOS::CPUVendor() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_vendor_;
}

void SystemSnapshotIOS::CPUFrequency(uint64_t* current_hz,
                                     uint64_t* max_hz) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): sysctlbyname hw.cpufrequency returns -1 on iOS/ARM64,
  // but consider recording this for X86_64 only.
  *current_hz = 0;
  *max_hz = 0;
}

uint32_t SystemSnapshotIOS::CPUX86Signature() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return 0;
}

uint64_t SystemSnapshotIOS::CPUX86Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return 0;
}

uint64_t SystemSnapshotIOS::CPUX86ExtendedFeatures() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return 0;
}

uint32_t SystemSnapshotIOS::CPUX86Leaf7Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return 0;
}

bool SystemSnapshotIOS::CPUX86SupportsDAZ() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return false;
}

SystemSnapshot::OperatingSystem SystemSnapshotIOS::GetOperatingSystem() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return kOperatingSystemIOS;
}

bool SystemSnapshotIOS::OSServer() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return false;
}

void SystemSnapshotIOS::OSVersion(int* major,
                                  int* minor,
                                  int* bugfix,
                                  std::string* build) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *major = os_version_major_;
  *minor = os_version_minor_;
  *bugfix = os_version_bugfix_;
  build->assign(os_version_build_);
}

std::string SystemSnapshotIOS::OSVersionFull() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return base::StringPrintf("%d.%d.%d %s",
                            os_version_major_,
                            os_version_minor_,
                            os_version_bugfix_,
                            os_version_build_.c_str());
}

std::string SystemSnapshotIOS::MachineDescription() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return machine_description_;
}

bool SystemSnapshotIOS::NXEnabled() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider using kern.nx when available (pre-iOS 13,
  // pre-OS X 10.15). Otherwise the bit is always enabled.
  return true;
}

void SystemSnapshotIOS::TimeZone(DaylightSavingTimeStatus* dst_status,
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
