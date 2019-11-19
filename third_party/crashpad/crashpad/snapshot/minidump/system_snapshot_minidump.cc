// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "snapshot/minidump/system_snapshot_minidump.h"

#include "snapshot/minidump/minidump_string_reader.h"

namespace crashpad {
namespace internal {

SystemSnapshotMinidump::SystemSnapshotMinidump()
    : SystemSnapshot(), minidump_system_info_(), initialized_() {}

SystemSnapshotMinidump::~SystemSnapshotMinidump() {}

bool SystemSnapshotMinidump::Initialize(FileReaderInterface* file_reader,
                                        RVA minidump_system_info_rva,
                                        const std::string& version) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  full_version_ = version;

  if (!file_reader->SeekSet(minidump_system_info_rva)) {
    return false;
  }

  if (!file_reader->ReadExactly(&minidump_system_info_,
                                sizeof(minidump_system_info_))) {
    return false;
  }

  if (!ReadMinidumpUTF8String(file_reader,
                              minidump_system_info_.CSDVersionRva,
                              &minidump_build_name_)) {
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

CPUArchitecture SystemSnapshotMinidump::GetCPUArchitecture() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  switch (minidump_system_info_.ProcessorArchitecture) {
    case kMinidumpCPUArchitectureAMD64:
      return kCPUArchitectureX86_64;
    case kMinidumpCPUArchitectureX86:
    case kMinidumpCPUArchitectureX86Win64:
      return kCPUArchitectureX86;
    case kMinidumpCPUArchitectureARM:
    case kMinidumpCPUArchitectureARM32Win64:
      return kCPUArchitectureARM;
    case kMinidumpCPUArchitectureARM64:
    case kMinidumpCPUArchitectureARM64Breakpad:
      return kCPUArchitectureARM64;
    case kMinidumpCPUArchitectureMIPS:
      return kCPUArchitectureMIPSEL;
    // No word on how MIPS64 is signalled

    default:
      return CPUArchitecture::kCPUArchitectureUnknown;
  }
}

uint32_t SystemSnapshotMinidump::CPURevision() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return minidump_system_info_.ProcessorRevision;
}

uint8_t SystemSnapshotMinidump::CPUCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return minidump_system_info_.NumberOfProcessors;
}

std::string SystemSnapshotMinidump::CPUVendor() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  if (GetCPUArchitecture() == kCPUArchitectureX86) {
    const char* ptr = reinterpret_cast<const char*>(
        minidump_system_info_.Cpu.X86CpuInfo.VendorId);
    return std::string(ptr, ptr + (3 * sizeof(uint32_t)));
  } else {
    return std::string();
  }
}

void SystemSnapshotMinidump::CPUFrequency(uint64_t* current_hz,
                                          uint64_t* max_hz) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
}

uint32_t SystemSnapshotMinidump::CPUX86Signature() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
  return 0;
}

uint64_t SystemSnapshotMinidump::CPUX86Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
  return 0;
}

uint64_t SystemSnapshotMinidump::CPUX86ExtendedFeatures() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
  return 0;
}

uint32_t SystemSnapshotMinidump::CPUX86Leaf7Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
  return 0;
}

bool SystemSnapshotMinidump::CPUX86SupportsDAZ() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
  return false;
}

SystemSnapshot::OperatingSystem SystemSnapshotMinidump::GetOperatingSystem()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  switch (minidump_system_info_.PlatformId) {
    case kMinidumpOSMacOSX:
      return OperatingSystem::kOperatingSystemMacOSX;
    case kMinidumpOSWin32s:
    case kMinidumpOSWin32Windows:
    case kMinidumpOSWin32NT:
      return OperatingSystem::kOperatingSystemWindows;
    case kMinidumpOSLinux:
      return OperatingSystem::kOperatingSystemLinux;
    case kMinidumpOSAndroid:
      return OperatingSystem::kOperatingSystemAndroid;
    case kMinidumpOSFuchsia:
      return OperatingSystem::kOperatingSystemFuchsia;
    default:
      return OperatingSystem::kOperatingSystemUnknown;
  }
}

bool SystemSnapshotMinidump::OSServer() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return minidump_system_info_.ProductType == kMinidumpOSTypeServer;
}

void SystemSnapshotMinidump::OSVersion(int* major,
                                       int* minor,
                                       int* bugfix,
                                       std::string* build) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *major = minidump_system_info_.MajorVersion;
  *minor = minidump_system_info_.MinorVersion;
  *bugfix = minidump_system_info_.BuildNumber;
  *build = minidump_build_name_;
}

std::string SystemSnapshotMinidump::OSVersionFull() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return full_version_;
}

std::string SystemSnapshotMinidump::MachineDescription() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
  return std::string();
}

bool SystemSnapshotMinidump::NXEnabled() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
  return false;
}

void SystemSnapshotMinidump::TimeZone(DaylightSavingTimeStatus* dst_status,
                                      int* standard_offset_seconds,
                                      int* daylight_offset_seconds,
                                      std::string* standard_name,
                                      std::string* daylight_name) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
}

}  // namespace internal
}  // namespace crashpad
