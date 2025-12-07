// Copyright 2014 The Crashpad Authors
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

#include "snapshot/mac/system_snapshot_mac.h"

#include <Availability.h>
#include <stddef.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <algorithm>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/scoped_clear_last_error.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "snapshot/cpu_context.h"
#include "snapshot/mac/process_reader_mac.h"
#include "snapshot/posix/timezone.h"
#include "util/mac/mac_util.h"
#include "util/mac/sysctl.h"
#include "util/numeric/in_range_cast.h"

namespace crashpad {

namespace {

template <typename T>
int ReadIntSysctlByName_NoLog(const char* name, T* value) {
  size_t value_len = sizeof(*value);
  return sysctlbyname(name, value, &value_len, nullptr, 0);
}

template <typename T>
T ReadIntSysctlByName(const char* name, T default_value) {
  T value;
  if (ReadIntSysctlByName_NoLog(name, &value) != 0) {
    PLOG(WARNING) << "sysctlbyname " << name;
    return default_value;
  }

  return value;
}

template <typename T>
T CastIntSysctlByName(const char* name, T default_value) {
  int int_value = ReadIntSysctlByName<int>(name, default_value);
  return InRangeCast<T>(int_value, default_value);
}

#if defined(ARCH_CPU_X86_FAMILY)
void CallCPUID(uint32_t leaf,
               uint32_t* eax,
               uint32_t* ebx,
               uint32_t* ecx,
               uint32_t* edx) {
  asm("cpuid"
      : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
      : "a"(leaf), "b"(0), "c"(0), "d"(0));
}
#endif

}  // namespace

namespace internal {

SystemSnapshotMac::SystemSnapshotMac()
    : SystemSnapshot(),
      os_version_full_(),
      os_version_build_(),
      process_reader_(nullptr),
      snapshot_time_(nullptr),
      os_version_major_(0),
      os_version_minor_(0),
      os_version_bugfix_(0),
      initialized_() {
}

SystemSnapshotMac::~SystemSnapshotMac() {
}

void SystemSnapshotMac::Initialize(ProcessReaderMac* process_reader,
                                   const timeval* snapshot_time) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_reader_ = process_reader;
  snapshot_time_ = snapshot_time;

  // MacOSVersionComponents() logs its own warnings if it can’t figure anything
  // out. It’s not fatal if this happens. The default values are reasonable.
  std::string os_version_string;
  MacOSVersionComponents(&os_version_major_,
                         &os_version_minor_,
                         &os_version_bugfix_,
                         &os_version_build_,
                         &os_version_string);

  std::string uname_string;
  utsname uts;
  if (uname(&uts) != 0) {
    PLOG(WARNING) << "uname";
  } else {
    uname_string = base::StringPrintf(
        "%s %s %s %s", uts.sysname, uts.release, uts.version, uts.machine);
  }

  if (!os_version_string.empty()) {
    if (!uname_string.empty()) {
      os_version_full_ = base::StringPrintf(
          "%s; %s", os_version_string.c_str(), uname_string.c_str());
    } else {
      os_version_full_ = os_version_string;
    }
  } else {
    os_version_full_ = uname_string;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
}

CPUArchitecture SystemSnapshotMac::GetCPUArchitecture() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  return process_reader_->Is64Bit() ? kCPUArchitectureX86_64
                                    : kCPUArchitectureX86;
#elif defined(ARCH_CPU_ARM64)
  return kCPUArchitectureARM64;
#else
#error port to your architecture
#endif
}

uint32_t SystemSnapshotMac::CPURevision() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  // machdep.cpu.family and machdep.cpu.model already take the extended family
  // and model IDs into account. See 10.9.2 xnu-2422.90.20/osfmk/i386/cpuid.c
  // cpuid_set_generic_info().
  uint16_t family = CastIntSysctlByName<uint16_t>("machdep.cpu.family", 0);
  uint8_t model = CastIntSysctlByName<uint8_t>("machdep.cpu.model", 0);
  uint8_t stepping = CastIntSysctlByName<uint8_t>("machdep.cpu.stepping", 0);

  return (family << 16) | (model << 8) | stepping;
#elif defined(ARCH_CPU_ARM64)
  // TODO(macos_arm64): Verify and test.
  return CastIntSysctlByName<uint32_t>("hw.cpufamily", 0);
#else
#error port to your architecture
#endif
}

uint8_t SystemSnapshotMac::CPUCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return CastIntSysctlByName<uint8_t>("hw.ncpu", 1);
}

std::string SystemSnapshotMac::CPUVendor() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  return ReadStringSysctlByName("machdep.cpu.vendor", true);
#elif defined(ARCH_CPU_ARM64)
  return ReadStringSysctlByName("machdep.cpu.brand_string", true);
#else
#error port to your architecture
#endif
}

void SystemSnapshotMac::CPUFrequency(
    uint64_t* current_hz, uint64_t* max_hz) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
#if defined(ARCH_CPU_X86_FAMILY)
  *current_hz = ReadIntSysctlByName<uint64_t>("hw.cpufrequency", 0);
  *max_hz = ReadIntSysctlByName<uint64_t>("hw.cpufrequency_max", 0);
#elif defined(ARCH_CPU_ARM64)
  // TODO(https://crashpad.chromium.org/bug/352): When production arm64
  // hardware is available, determine whether CPU frequency is visible anywhere
  // (likely via a sysctl or via IOKit) and use it if feasible.
  *current_hz = 0;
  *max_hz = 0;
#else
#error port to your architecture
#endif
}

uint32_t SystemSnapshotMac::CPUX86Signature() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  return ReadIntSysctlByName<uint32_t>("machdep.cpu.signature", 0);
#else
  NOTREACHED();
#endif
}

uint64_t SystemSnapshotMac::CPUX86Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  return ReadIntSysctlByName<uint64_t>("machdep.cpu.feature_bits", 0);
#else
  NOTREACHED();
#endif
}

uint64_t SystemSnapshotMac::CPUX86ExtendedFeatures() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  return ReadIntSysctlByName<uint64_t>("machdep.cpu.extfeature_bits", 0);
#else
  NOTREACHED();
#endif
}

uint32_t SystemSnapshotMac::CPUX86Leaf7Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  // The machdep.cpu.leaf7_feature_bits sysctl isn’t supported prior to OS X
  // 10.7, so read this by calling cpuid directly.
  //
  // machdep.cpu.max_basic could be used to check whether to read the leaf, but
  // that sysctl isn’t supported prior to Mac OS X 10.6, so read the maximum
  // basic leaf by calling cpuid directly as well. All CPUs that Apple is known
  // to have shipped should support a maximum basic leaf value of at least 0xa.
  uint32_t eax, ebx, ecx, edx;
  CallCPUID(0, &eax, &ebx, &ecx, &edx);
  if (eax < 7) {
    return 0;
  }

  CallCPUID(7, &eax, &ebx, &ecx, &edx);
  return ebx;
#else
  NOTREACHED();
#endif
}

bool SystemSnapshotMac::CPUX86SupportsDAZ() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

#if defined(ARCH_CPU_X86_FAMILY)
  // The correct way to check for denormals-as-zeros (DAZ) support is to examine
  // mxcsr mask, which can be done with fxsave. See Intel Software Developer’s
  // Manual, Volume 1: Basic Architecture (253665-051), 11.6.3 “Checking for the
  // DAZ Flag in the MXCSR Register”. Note that since this function tests for
  // DAZ support in the CPU, it checks the mxcsr mask. Testing mxcsr would
  // indicate whether DAZ is actually enabled, which is a per-thread context
  // concern.
  //
  // All CPUs that Apple is known to have shipped should support DAZ.

  // Test for fxsave support.
  uint64_t features = CPUX86Features();
  if (!(features & (UINT64_C(1) << 24))) {
    return false;
  }

  // Call fxsave.
#if defined(ARCH_CPU_X86)
  CPUContextX86::Fxsave fxsave __attribute__((aligned(16))) = {};
#elif defined(ARCH_CPU_X86_64)
  CPUContextX86_64::Fxsave fxsave __attribute__((aligned(16))) = {};
#endif
  static_assert(sizeof(fxsave) == 512, "fxsave size");
  static_assert(offsetof(decltype(fxsave), mxcsr_mask) == 28,
                "mxcsr_mask offset");
  asm("fxsave %0" : "=m"(fxsave));

  // Test the DAZ bit.
  return fxsave.mxcsr_mask & (1 << 6);
#else
  NOTREACHED();
#endif
}

SystemSnapshot::OperatingSystem SystemSnapshotMac::GetOperatingSystem() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return kOperatingSystemMacOSX;
}

bool SystemSnapshotMac::OSServer() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return false;
}

void SystemSnapshotMac::OSVersion(int* major,
                                  int* minor,
                                  int* bugfix,
                                  std::string* build) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *major = os_version_major_;
  *minor = os_version_minor_;
  *bugfix = os_version_bugfix_;
  build->assign(os_version_build_);
}

std::string SystemSnapshotMac::OSVersionFull() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return os_version_full_;
}

std::string SystemSnapshotMac::MachineDescription() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  std::string model;
  std::string board_id;
  MacModelAndBoard(&model, &board_id);

  if (!model.empty()) {
    if (!board_id.empty()) {
      return base::StringPrintf("%s (%s)", model.c_str(), board_id.c_str());
    }
    return model;
  }
  if (!board_id.empty()) {
    return base::StringPrintf("(%s)", board_id.c_str());
  }
  return std::string();
}

bool SystemSnapshotMac::NXEnabled() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  int value;
  if (ReadIntSysctlByName_NoLog("kern.nx", &value) != 0) {
    {
      // Support for the kern.nx sysctlbyname is compiled out of production
      // kernels on macOS 10.14.5 and later, although it’s available in
      // development and debug kernels. Compare 10.14.3
      // xnu-4903.241.1/bsd/kern/kern_sysctl.c to 10.15.0
      // xnu-6153.11.26/bsd/kern/kern_sysctl.c (10.14.4 and 10.14.5 xnu source
      // are not yet available). In newer production kernels, NX is always
      // enabled. See 10.15.0 xnu-6153.11.26/osfmk/x86_64/pmap.c nx_enabled.
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_14
      const bool nx_always_enabled = true;
#else  // DT >= 10.14
      base::ScopedClearLastError reset_errno;
      const bool nx_always_enabled = MacOSVersionNumber() >= 10'14'00;
#endif  // DT >= 10.14
      if (nx_always_enabled) {
        return true;
      }
    }

    // Even if sysctlbyname should have worked, NX is enabled by default in all
    // supported configurations, so return true even while warning.
    PLOG(WARNING) << "sysctlbyname kern.nx";
    return true;
  }

  return value;
}

void SystemSnapshotMac::TimeZone(DaylightSavingTimeStatus* dst_status,
                                 int* standard_offset_seconds,
                                 int* daylight_offset_seconds,
                                 std::string* standard_name,
                                 std::string* daylight_name) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  internal::TimeZone(*snapshot_time_,
                     dst_status,
                     standard_offset_seconds,
                     daylight_offset_seconds,
                     standard_name,
                     daylight_name);
}

uint64_t SystemSnapshotMac::AddressMask() const {
  uint64_t mask = 0;
#if defined(ARCH_CPU_ARM64)
  // `machdep.virtual_address_size` is the number of addressable bits in
  // userspace virtual addresses
  uint8_t addressable_bits =
      CastIntSysctlByName<uint8_t>("machdep.virtual_address_size", 0);
  if (addressable_bits) {
    mask = ~((1UL << addressable_bits) - 1);
  }
#endif
  return mask;
}

}  // namespace internal
}  // namespace crashpad
