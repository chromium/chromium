// Copyright 2021 The Crashpad Authors
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

#include "client/ios_handler/in_process_intermediate_dump_handler.h"

#include <mach-o/dyld_images.h>
#include <mach-o/nlist.h>
#include <stdint.h>
#include <string.h>
#include <sys/sysctl.h>
#include <time.h>

#include <iterator>
#include <optional>

#include "base/check_op.h"
#include "build/build_config.h"
#include "snapshot/snapshot_constants.h"
#include "util/ios/ios_intermediate_dump_writer.h"
#include "util/ios/raw_logging.h"
#include "util/ios/scoped_vm_map.h"
#include "util/ios/scoped_vm_read.h"
#include "util/synchronization/scoped_spin_guard.h"

namespace crashpad {
namespace internal {

namespace {

#if defined(ARCH_CPU_X86_64)
const thread_state_flavor_t kThreadStateFlavor = x86_THREAD_STATE64;
const thread_state_flavor_t kFloatStateFlavor = x86_FLOAT_STATE64;
const thread_state_flavor_t kDebugStateFlavor = x86_DEBUG_STATE64;
using thread_state_type = x86_thread_state64_t;
#elif defined(ARCH_CPU_ARM64)
const thread_state_flavor_t kThreadStateFlavor = ARM_THREAD_STATE64;
const thread_state_flavor_t kFloatStateFlavor = ARM_NEON_STATE64;
const thread_state_flavor_t kDebugStateFlavor = ARM_DEBUG_STATE64;
using thread_state_type = arm_thread_state64_t;
#endif

// From snapshot/mac/process_types/crashreporterclient.proctype
struct crashreporter_annotations_t {
  uint64_t version;
  uint64_t message;
  uint64_t signature_string;
  uint64_t backtrace;
  uint64_t message2;
  uint64_t thread;
  uint64_t dialog_mode;
  uint64_t abort_cause;
};

//! \brief Manage memory and ports after calling `task_threads`.
class ScopedTaskThreads {
 public:
  explicit ScopedTaskThreads(thread_act_array_t threads,
                             mach_msg_type_number_t thread_count)
      : threads_(threads), thread_count_(thread_count) {}

  ScopedTaskThreads(const ScopedTaskThreads&) = delete;
  ScopedTaskThreads& operator=(const ScopedTaskThreads&) = delete;

  ~ScopedTaskThreads() {
    for (uint32_t thread_index = 0; thread_index < thread_count_;
         ++thread_index) {
      mach_port_deallocate(mach_task_self(), threads_[thread_index]);
    }
    vm_deallocate(mach_task_self(),
                  reinterpret_cast<vm_address_t>(threads_),
                  sizeof(thread_t) * thread_count_);
  }

 private:
  thread_act_array_t threads_;
  mach_msg_type_number_t thread_count_;
};

//! \brief Log \a key as a string.
void WriteError(IntermediateDumpKey key) {
  CRASHPAD_RAW_LOG("Unable to write key");
  switch (key) {
// clang-format off
#define CASE_KEY(Name, Value)       \
    case IntermediateDumpKey::Name: \
      CRASHPAD_RAW_LOG(#Name);      \
      break;
    INTERMEDIATE_DUMP_KEYS(CASE_KEY)
#undef CASE_KEY
    // clang-format on
  }
}

//! \brief Call AddProperty with raw error log.
//!
//! \param[in] writer The dump writer
//! \param[in] key The key to write.
//! \param[in] value Memory to be written.
//! \param[in] count Length of \a value.
template <typename T>
void WriteProperty(IOSIntermediateDumpWriter* writer,
                   IntermediateDumpKey key,
                   const T* value,
                   size_t count = 1) {
  if (!writer->AddProperty(key, value, count))
    WriteError(key);
}

//! \brief Call AddPropertyBytes with raw error log.
//!
//! \param[in] writer The dump writer
//! \param[in] key The key to write.
//! \param[in] value Memory to be written.
//! \param[in] value_length Length of \a data.
void WritePropertyBytes(IOSIntermediateDumpWriter* writer,
                        IntermediateDumpKey key,
                        const void* value,
                        size_t value_length) {
  if (!writer->AddPropertyBytes(key, value, value_length))
    WriteError(key);
}

//! \brief Call AddPropertyCString with raw error log.
//!
//! \param[in] writer The dump writer
//! \param[in] key The key to write.
//! \param[in] max_length The maximum string length.
//! \param[in] value Memory to be written.
void WritePropertyCString(IOSIntermediateDumpWriter* writer,
                          IntermediateDumpKey key,
                          size_t max_length,
                          const char* value) {
  if (!writer->AddPropertyCString(key, max_length, value))
    WriteError(key);
}

kern_return_t MachVMRegionRecurseDeepest(task_t task,
                                         vm_address_t* address,
                                         vm_size_t* size,
                                         natural_t* depth,
                                         vm_prot_t* protection,
                                         unsigned int* user_tag) {
  vm_region_submap_short_info_64 submap_info;
  mach_msg_type_number_t count = VM_REGION_SUBMAP_SHORT_INFO_COUNT_64;
  while (true) {
    // Note: vm_region_recurse() would be fine here, but it does not provide
    // VM_REGION_SUBMAP_SHORT_INFO_COUNT.
    kern_return_t kr = vm_region_recurse_64(
        task,
        address,
        size,
        depth,
        reinterpret_cast<vm_region_recurse_info_t>(&submap_info),
        &count);
    if (kr != KERN_SUCCESS) {
      CRASHPAD_RAW_LOG_ERROR(kr, "vm_region_recurse_64");
      return kr;
    }

    if (!submap_info.is_submap) {
      *protection = submap_info.protection;
      *user_tag = submap_info.user_tag;
      return KERN_SUCCESS;
    }

    ++*depth;
  }
}

//! \brief Adjusts the region for the red zone, if the ABI requires one.
//!
//! This method performs red zone calculation for CalculateStackRegion(). Its
//! parameters are local variables used within that method, and may be
//! modified as needed.
//!
//! Where a red zone is required, the region of memory captured for a thread’s
//! stack will be extended to include the red zone below the stack pointer,
//! provided that such memory is mapped, readable, and has the correct user
//! tag value. If these conditions cannot be met fully, as much of the red
//! zone will be captured as is possible while meeting these conditions.
//!
//! \param[in,out] start_address The base address of the region to begin
//!     capturing stack memory from. On entry, \a start_address is the stack
//!     pointer. On return, \a start_address may be decreased to encompass a
//!     red zone.
//! \param[in,out] region_base The base address of the region that contains
//!     stack memory. This is distinct from \a start_address in that \a
//!     region_base will be page-aligned. On entry, \a region_base is the
//!     base address of a region that contains \a start_address. On return,
//!     if \a start_address is decremented and is outside of the region
//!     originally described by \a region_base, \a region_base will also be
//!     decremented appropriately.
//! \param[in,out] region_size The size of the region that contains stack
//!     memory. This region begins at \a region_base. On return, if \a
//!     region_base is decremented, \a region_size will be incremented
//!     appropriately.
//! \param[in] user_tag The Mach VM system’s user tag for the region described
//!     by the initial values of \a region_base and \a region_size. The red
//!     zone will only be allowed to extend out of the region described by
//!     these initial values if the user tag is appropriate for stack memory
//!     and the expanded region has the same user tag value.
void LocateRedZone(vm_address_t* const start_address,
                   vm_address_t* const region_base,
                   vm_address_t* const region_size,
                   const unsigned int user_tag) {
  // x86_64 has a red zone. See AMD64 ABI 0.99.8,
  // https://gitlab.com/x86-psABIs/x86-64-ABI/-/wikis/uploads/01de35b2c8adc7545de52604cc45d942/x86-64-psABI-2021-05-20.pdf#page=23.
  // section 3.2.2, “The Stack Frame”.
  // So does ARM64,
  // https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms#Respect-the-Stacks-Red-Zone
  // section "Respect the Stack’s Red Zone".
  constexpr vm_size_t kRedZoneSize = 128;
  vm_address_t red_zone_base =
      *start_address >= kRedZoneSize ? *start_address - kRedZoneSize : 0;
  bool red_zone_ok = false;
  if (red_zone_base >= *region_base) {
    // The red zone is within the region already discovered.
    red_zone_ok = true;
  } else if (red_zone_base < *region_base && user_tag == VM_MEMORY_STACK) {
    // Probe to see if there’s a region immediately below the one already
    // discovered.
    vm_address_t red_zone_region_base = red_zone_base;
    vm_size_t red_zone_region_size;
    natural_t red_zone_depth = 0;
    vm_prot_t red_zone_protection;
    unsigned int red_zone_user_tag;
    kern_return_t kr = MachVMRegionRecurseDeepest(mach_task_self(),
                                                  &red_zone_region_base,
                                                  &red_zone_region_size,
                                                  &red_zone_depth,
                                                  &red_zone_protection,
                                                  &red_zone_user_tag);
    if (kr != KERN_SUCCESS) {
      *start_address = *region_base;
    } else if (red_zone_region_base + red_zone_region_size == *region_base &&
               (red_zone_protection & VM_PROT_READ) != 0 &&
               red_zone_user_tag == user_tag) {
      // The region containing the red zone is immediately below the region
      // already found, it’s readable (not the guard region), and it has the
      // same user tag as the region already found, so merge them.
      red_zone_ok = true;
      *region_base -= red_zone_region_size;
      *region_size += red_zone_region_size;
    }
  }

  if (red_zone_ok) {
    // Begin capturing from the base of the red zone (but not the entire
    // region that encompasses the red zone).
    *start_address = red_zone_base;
  } else {
    // The red zone would go lower into another region in memory, but no
    // region was found. Memory can only be captured to an address as low as
    // the base address of the region already found.
    *start_address = *region_base;
  }
}

//! \brief Calculates the base address and size of the region used as a
//!     thread’s stack.
//!
//! The region returned by this method may be formed by merging multiple
//! adjacent regions in a process’ memory map if appropriate. The base address
//! of the returned region may be lower than the \a stack_pointer passed in
//! when the ABI mandates a red zone below the stack pointer.
//!
//! \param[in] stack_pointer The stack pointer, referring to the top (lowest
//!     address) of a thread’s stack.
//! \param[out] stack_region_size The size of the memory region used as the
//!     thread’s stack.
//!
//! \return The base address (lowest address) of the memory region used as the
//!     thread’s stack.
vm_address_t CalculateStackRegion(vm_address_t stack_pointer,
                                  vm_size_t* stack_region_size) {
  // For pthreads, it may be possible to compute the stack region based on the
  // internal _pthread::stackaddr and _pthread::stacksize. The _pthread struct
  // for a thread can be located at TSD slot 0, or the known offsets of
  // stackaddr and stacksize from the TSD area could be used.
  vm_address_t region_base = stack_pointer;
  vm_size_t region_size;
  natural_t depth = 0;
  vm_prot_t protection;
  unsigned int user_tag;
  kern_return_t kr = MachVMRegionRecurseDeepest(mach_task_self(),
                                                &region_base,
                                                &region_size,
                                                &depth,
                                                &protection,
                                                &user_tag);
  if (kr != KERN_SUCCESS) {
    CRASHPAD_RAW_LOG_ERROR(kr, "MachVMRegionRecurseDeepest");
    *stack_region_size = 0;
    return 0;
  }

  if (region_base > stack_pointer) {
    // There’s nothing mapped at the stack pointer’s address. Something may have
    // trashed the stack pointer. Note that this shouldn’t happen for a normal
    // stack guard region violation because the guard region is mapped but has
    // VM_PROT_NONE protection.
    *stack_region_size = 0;
    return 0;
  }

  vm_address_t start_address = stack_pointer;

  if ((protection & VM_PROT_READ) == 0) {
    // If the region isn’t readable, the stack pointer probably points to the
    // guard region. Don’t include it as part of the stack, and don’t include
    // anything at any lower memory address. The code below may still possibly
    // find the real stack region at a memory address higher than this region.
    start_address = region_base + region_size;
  } else {
    // If the ABI requires a red zone, adjust the region to include it if
    // possible.
    LocateRedZone(&start_address, &region_base, &region_size, user_tag);

    // Regardless of whether the ABI requires a red zone, capture up to
    // kExtraCaptureSize additional bytes of stack, but only if present in the
    // region that was already found.
    constexpr vm_size_t kExtraCaptureSize = 128;
    start_address = std::max(start_address >= kExtraCaptureSize
                                 ? start_address - kExtraCaptureSize
                                 : start_address,
                             region_base);

    // Align start_address to a 16-byte boundary, which can help readers by
    // ensuring that data is aligned properly. This could page-align instead,
    // but that might be wasteful.
    constexpr vm_size_t kDesiredAlignment = 16;
    start_address &= ~(kDesiredAlignment - 1);
    DCHECK_GE(start_address, region_base);
  }

  region_size -= (start_address - region_base);
  region_base = start_address;

  vm_size_t total_region_size = region_size;

  // The stack region may have gotten split up into multiple abutting regions.
  // Try to coalesce them. This frequently happens for the main thread’s stack
  // when setrlimit(RLIMIT_STACK, …) is called. It may also happen if a region
  // is split up due to an mprotect() or vm_protect() call.
  //
  // Stack regions created by the kernel and the pthreads library will be marked
  // with the VM_MEMORY_STACK user tag. Scanning for multiple adjacent regions
  // with the same tag should find an entire stack region. Checking that the
  // protection on individual regions is not VM_PROT_NONE should guarantee that
  // this algorithm doesn’t collect map entries belonging to another thread’s
  // stack: well-behaved stacks (such as those created by the kernel and the
  // pthreads library) have VM_PROT_NONE guard regions at their low-address
  // ends.
  //
  // Other stack regions may not be so well-behaved and thus if user_tag is not
  // VM_MEMORY_STACK, the single region that was found is used as-is without
  // trying to merge it with other adjacent regions.
  if (user_tag == VM_MEMORY_STACK) {
    vm_address_t try_address = region_base;
    vm_address_t original_try_address;

    while (try_address += region_size,
           original_try_address = try_address,
           (kr = MachVMRegionRecurseDeepest(mach_task_self(),
                                            &try_address,
                                            &region_size,
                                            &depth,
                                            &protection,
                                            &user_tag) == KERN_SUCCESS) &&
               try_address == original_try_address &&
               (protection & VM_PROT_READ) != 0 &&
               user_tag == VM_MEMORY_STACK) {
      total_region_size += region_size;
    }

    if (kr != KERN_SUCCESS && kr != KERN_INVALID_ADDRESS) {
      // Tolerate KERN_INVALID_ADDRESS because it will be returned when there
      // are no more regions in the map at or above the specified |try_address|.
      CRASHPAD_RAW_LOG_ERROR(kr, "MachVMRegionRecurseDeepest");
    }
  }

  *stack_region_size = total_region_size;
  return region_base;
}

//! \brief Write data around \a address to intermediate dump. Must be called
//!    from within a ScopedArray.
void MaybeCaptureMemoryAround(IOSIntermediateDumpWriter* writer,
                              uint64_t address) {
  constexpr uint64_t non_address_offset = 0x10000;
  if (address < non_address_offset)
    return;

  constexpr uint64_t max_address = std::numeric_limits<uint64_t>::max();

  if (address > max_address - non_address_offset)
    return;

  constexpr uint64_t kRegisterByteOffset = 128;
  const uint64_t target = address - kRegisterByteOffset;
  constexpr uint64_t size = 512;
  static_assert(kRegisterByteOffset <= size / 2, "negative offset too large");

  IOSIntermediateDumpWriter::ScopedArrayMap memory_region(writer);
  WriteProperty(
      writer, IntermediateDumpKey::kThreadContextMemoryRegionAddress, &target);
  // Don't use WritePropertyBytes, this one will fail regularly if |target|
  // cannot be read.
  writer->AddPropertyBytes(IntermediateDumpKey::kThreadContextMemoryRegionData,
                           reinterpret_cast<const void*>(target),
                           size);
}

void CaptureMemoryPointedToByThreadState(IOSIntermediateDumpWriter* writer,
                                         thread_state_type thread_state) {
  IOSIntermediateDumpWriter::ScopedArray memory_regions(
      writer, IntermediateDumpKey::kThreadContextMemoryRegions);

#if defined(ARCH_CPU_X86_64)
  MaybeCaptureMemoryAround(writer, thread_state.__rax);
  MaybeCaptureMemoryAround(writer, thread_state.__rbx);
  MaybeCaptureMemoryAround(writer, thread_state.__rcx);
  MaybeCaptureMemoryAround(writer, thread_state.__rdx);
  MaybeCaptureMemoryAround(writer, thread_state.__rdi);
  MaybeCaptureMemoryAround(writer, thread_state.__rsi);
  MaybeCaptureMemoryAround(writer, thread_state.__rbp);
  MaybeCaptureMemoryAround(writer, thread_state.__r8);
  MaybeCaptureMemoryAround(writer, thread_state.__r9);
  MaybeCaptureMemoryAround(writer, thread_state.__r10);
  MaybeCaptureMemoryAround(writer, thread_state.__r11);
  MaybeCaptureMemoryAround(writer, thread_state.__r12);
  MaybeCaptureMemoryAround(writer, thread_state.__r13);
  MaybeCaptureMemoryAround(writer, thread_state.__r14);
  MaybeCaptureMemoryAround(writer, thread_state.__r15);
  MaybeCaptureMemoryAround(writer, thread_state.__rip);
#elif defined(ARCH_CPU_ARM_FAMILY)
  MaybeCaptureMemoryAround(writer, arm_thread_state64_get_pc(thread_state));
  for (size_t i = 0; i < std::size(thread_state.__x); ++i) {
    MaybeCaptureMemoryAround(writer, thread_state.__x[i]);
  }
#endif
}

void WriteCrashpadSimpleAnnotationsDictionary(IOSIntermediateDumpWriter* writer,
                                              CrashpadInfo* crashpad_info) {
  if (!crashpad_info->simple_annotations())
    return;

  ScopedVMRead<SimpleStringDictionary> simple_annotations;
  if (!simple_annotations.Read(crashpad_info->simple_annotations())) {
    CRASHPAD_RAW_LOG("Unable to read simple annotations.");
    return;
  }

  const size_t count = simple_annotations->GetCount();
  if (!count)
    return;

  IOSIntermediateDumpWriter::ScopedArray annotations_array(
      writer, IntermediateDumpKey::kAnnotationsSimpleMap);

  SimpleStringDictionary::Entry* entries =
      reinterpret_cast<SimpleStringDictionary::Entry*>(
          simple_annotations.get());
  for (size_t index = 0; index < count; index++) {
    IOSIntermediateDumpWriter::ScopedArrayMap annotation_map(writer);
    const auto& entry = entries[index];
    size_t key_length = strnlen(entry.key, sizeof(entry.key));
    WritePropertyBytes(writer,
                       IntermediateDumpKey::kAnnotationName,
                       reinterpret_cast<const void*>(entry.key),
                       key_length);
    size_t value_length = strnlen(entry.value, sizeof(entry.value));
    WritePropertyBytes(writer,
                       IntermediateDumpKey::kAnnotationValue,
                       reinterpret_cast<const void*>(entry.value),
                       value_length);
  }
}

void WriteAppleCrashReporterAnnotations(
    IOSIntermediateDumpWriter* writer,
    crashreporter_annotations_t* crash_info) {
  // This number was totally made up out of nowhere, but it seems prudent to
  // enforce some limit.
  constexpr size_t kMaxMessageSize = 1024;
  IOSIntermediateDumpWriter::ScopedMap annotation_map(
      writer, IntermediateDumpKey::kAnnotationsCrashInfo);
  if (crash_info->message) {
    const size_t message_len = strnlen(
        reinterpret_cast<const char*>(crash_info->message), kMaxMessageSize);
    WritePropertyBytes(writer,
                       IntermediateDumpKey::kAnnotationsCrashInfoMessage1,
                       reinterpret_cast<const void*>(crash_info->message),
                       message_len);
  }
  if (crash_info->message2) {
    const size_t message_len = strnlen(
        reinterpret_cast<const char*>(crash_info->message2), kMaxMessageSize);
    WritePropertyBytes(writer,
                       IntermediateDumpKey::kAnnotationsCrashInfoMessage2,
                       reinterpret_cast<const void*>(crash_info->message2),
                       message_len);
  }
}

}  // namespace

// static
void InProcessIntermediateDumpHandler::WriteHeader(
    IOSIntermediateDumpWriter* writer) {
  static constexpr uint8_t version = 1;
  WriteProperty(writer, IntermediateDumpKey::kVersion, &version);
}

// static
void InProcessIntermediateDumpHandler::WriteProcessInfo(
    IOSIntermediateDumpWriter* writer,
    const std::map<std::string, std::string>& annotations) {
  IOSIntermediateDumpWriter::ScopedMap process_map(
      writer, IntermediateDumpKey::kProcessInfo);

  timeval snapshot_time;
  if (gettimeofday(&snapshot_time, nullptr) == 0) {
    WriteProperty(writer, IntermediateDumpKey::kSnapshotTime, &snapshot_time);
  } else {
    CRASHPAD_RAW_LOG("gettimeofday");
  }

  // Used by pid, parent pid and snapshot time.
  kinfo_proc kern_proc_info;
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  size_t len = sizeof(kern_proc_info);
  if (sysctl(mib, std::size(mib), &kern_proc_info, &len, nullptr, 0) == 0) {
    WriteProperty(
        writer, IntermediateDumpKey::kPID, &kern_proc_info.kp_proc.p_pid);
    WriteProperty(writer,
                  IntermediateDumpKey::kParentPID,
                  &kern_proc_info.kp_eproc.e_ppid);
    WriteProperty(writer,
                  IntermediateDumpKey::kStartTime,
                  &kern_proc_info.kp_proc.p_starttime);
  } else {
    CRASHPAD_RAW_LOG("sysctl kern_proc_info");
  }

  // Used by user time and system time.
  mach_task_basic_info task_basic_info;
  mach_msg_type_number_t task_basic_info_count = MACH_TASK_BASIC_INFO_COUNT;
  kern_return_t kr = task_info(mach_task_self(),
                               MACH_TASK_BASIC_INFO,
                               reinterpret_cast<task_info_t>(&task_basic_info),
                               &task_basic_info_count);
  if (kr == KERN_SUCCESS) {
    IOSIntermediateDumpWriter::ScopedMap task_info(
        writer, IntermediateDumpKey::kTaskBasicInfo);

    WriteProperty(
        writer, IntermediateDumpKey::kUserTime, &task_basic_info.user_time);
    WriteProperty(
        writer, IntermediateDumpKey::kSystemTime, &task_basic_info.system_time);
  } else {
    CRASHPAD_RAW_LOG("task_info task_basic_info");
  }

  task_thread_times_info_data_t task_thread_times;
  mach_msg_type_number_t task_thread_times_count = TASK_THREAD_TIMES_INFO_COUNT;
  kr = task_info(mach_task_self(),
                 TASK_THREAD_TIMES_INFO,
                 reinterpret_cast<task_info_t>(&task_thread_times),
                 &task_thread_times_count);
  if (kr == KERN_SUCCESS) {
    IOSIntermediateDumpWriter::ScopedMap task_thread_times_map(
        writer, IntermediateDumpKey::kTaskThreadTimes);

    WriteProperty(
        writer, IntermediateDumpKey::kUserTime, &task_thread_times.user_time);
    WriteProperty(writer,
                  IntermediateDumpKey::kSystemTime,
                  &task_thread_times.system_time);
  } else {
    CRASHPAD_RAW_LOG("task_info thread_times_info");
  }

  if (!annotations.empty()) {
    IOSIntermediateDumpWriter::ScopedArray simple_annotations_array(
        writer, IntermediateDumpKey::kAnnotationsSimpleMap);
    for (const auto& annotation_pair : annotations) {
      const std::string& key = annotation_pair.first;
      const std::string& value = annotation_pair.second;
      IOSIntermediateDumpWriter::ScopedArrayMap annotation_map(writer);
      WriteProperty(writer,
                    IntermediateDumpKey::kAnnotationName,
                    key.c_str(),
                    key.length());
      WriteProperty(writer,
                    IntermediateDumpKey::kAnnotationValue,
                    value.c_str(),
                    value.length());
    }
  }
}

// static
void InProcessIntermediateDumpHandler::WriteSystemInfo(
    IOSIntermediateDumpWriter* writer,
    const IOSSystemDataCollector& system_data,
    uint64_t report_time_nanos) {
  IOSIntermediateDumpWriter::ScopedMap system_map(
      writer, IntermediateDumpKey::kSystemInfo);

  const std::string& machine_description = system_data.MachineDescription();
  WriteProperty(writer,
                IntermediateDumpKey::kMachineDescription,
                machine_description.c_str(),
                machine_description.length());
  int os_version_major;
  int os_version_minor;
  int os_version_bugfix;
  system_data.OSVersion(
      &os_version_major, &os_version_minor, &os_version_bugfix);
  WriteProperty(
      writer, IntermediateDumpKey::kOSVersionMajor, &os_version_major);
  WriteProperty(
      writer, IntermediateDumpKey::kOSVersionMinor, &os_version_minor);
  WriteProperty(
      writer, IntermediateDumpKey::kOSVersionBugfix, &os_version_bugfix);
  const std::string& os_version_build = system_data.Build();
  WriteProperty(writer,
                IntermediateDumpKey::kOSVersionBuild,
                os_version_build.c_str(),
                os_version_build.length());

  int cpu_count = system_data.ProcessorCount();
  WriteProperty(writer, IntermediateDumpKey::kCpuCount, &cpu_count);
  const std::string& cpu_vendor = system_data.CPUVendor();
  WriteProperty(writer,
                IntermediateDumpKey::kCpuVendor,
                cpu_vendor.c_str(),
                cpu_vendor.length());

  bool has_daylight_saving_time = system_data.HasDaylightSavingTime();
  WriteProperty(writer,
                IntermediateDumpKey::kHasDaylightSavingTime,
                &has_daylight_saving_time);
  bool is_daylight_saving_time = system_data.IsDaylightSavingTime();
  WriteProperty(writer,
                IntermediateDumpKey::kIsDaylightSavingTime,
                &is_daylight_saving_time);
  int standard_offset_seconds = system_data.StandardOffsetSeconds();
  WriteProperty(writer,
                IntermediateDumpKey::kStandardOffsetSeconds,
                &standard_offset_seconds);
  int daylight_offset_seconds = system_data.DaylightOffsetSeconds();
  WriteProperty(writer,
                IntermediateDumpKey::kDaylightOffsetSeconds,
                &daylight_offset_seconds);
  const std::string& standard_name = system_data.StandardName();
  WriteProperty(writer,
                IntermediateDumpKey::kStandardName,
                standard_name.c_str(),
                standard_name.length());
  const std::string& daylight_name = system_data.DaylightName();
  WriteProperty(writer,
                IntermediateDumpKey::kDaylightName,
                daylight_name.c_str(),
                daylight_name.length());
  uint64_t address_mask = system_data.AddressMask();
  WriteProperty(writer, IntermediateDumpKey::kAddressMask, &address_mask);

  vm_size_t page_size;
  host_page_size(mach_host_self(), &page_size);
  WriteProperty(writer, IntermediateDumpKey::kPageSize, &page_size);

  mach_msg_type_number_t host_size =
      sizeof(vm_statistics_data_t) / sizeof(integer_t);
  vm_statistics_data_t vm_stat;
  kern_return_t kr = host_statistics(mach_host_self(),
                                     HOST_VM_INFO,
                                     reinterpret_cast<host_info_t>(&vm_stat),
                                     &host_size);
  if (kr == KERN_SUCCESS) {
    IOSIntermediateDumpWriter::ScopedMap vm_stat_map(
        writer, IntermediateDumpKey::kVMStat);

    WriteProperty(writer, IntermediateDumpKey::kActive, &vm_stat.active_count);
    WriteProperty(
        writer, IntermediateDumpKey::kInactive, &vm_stat.inactive_count);
    WriteProperty(writer, IntermediateDumpKey::kWired, &vm_stat.wire_count);
    WriteProperty(writer, IntermediateDumpKey::kFree, &vm_stat.free_count);
  } else {
    CRASHPAD_RAW_LOG("host_statistics");
  }

  uint64_t crashpad_uptime_nanos =
      report_time_nanos - system_data.InitializationTime();
  WriteProperty(
      writer, IntermediateDumpKey::kCrashpadUptime, &crashpad_uptime_nanos);
}

// static
void InProcessIntermediateDumpHandler::WriteThreadInfo(
    IOSIntermediateDumpWriter* writer,
    const uint64_t* frames,
    const size_t num_frames) {
  IOSIntermediateDumpWriter::ScopedArray thread_array(
      writer, IntermediateDumpKey::kThreads);

  // Exception thread ID.
#if defined(ARCH_CPU_ARM64)
  uint64_t exception_thread_id = 0;
#endif
  thread_identifier_info identifier_info;
  mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
  kern_return_t kr =
      thread_info(mach_thread_self(),
                  THREAD_IDENTIFIER_INFO,
                  reinterpret_cast<thread_info_t>(&identifier_info),
                  &count);
  if (kr == KERN_SUCCESS) {
#if defined(ARCH_CPU_ARM64)
    exception_thread_id = identifier_info.thread_id;
#endif
  } else {
    CRASHPAD_RAW_LOG_ERROR(kr, "thread_info::THREAD_IDENTIFIER_INFO");
  }

  mach_msg_type_number_t thread_count = 0;
  thread_act_array_t threads;
  kr = task_threads(mach_task_self(), &threads, &thread_count);
  if (kr != KERN_SUCCESS) {
    CRASHPAD_RAW_LOG_ERROR(kr, "task_threads");
  }
  ScopedTaskThreads threads_vm_owner(threads, thread_count);

  for (uint32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
    IOSIntermediateDumpWriter::ScopedArrayMap thread_map(writer);
    thread_t thread = threads[thread_index];

    thread_basic_info basic_info;
    count = THREAD_BASIC_INFO_COUNT;
    kr = thread_info(thread,
                     THREAD_BASIC_INFO,
                     reinterpret_cast<thread_info_t>(&basic_info),
                     &count);
    if (kr == KERN_SUCCESS) {
      WriteProperty(writer,
                    IntermediateDumpKey::kSuspendCount,
                    &basic_info.suspend_count);
    } else {
      CRASHPAD_RAW_LOG_ERROR(kr, "thread_info::THREAD_BASIC_INFO");
    }

    thread_extended_info extended_info;
    count = THREAD_EXTENDED_INFO_COUNT;
    kr = thread_info(thread,
                     THREAD_EXTENDED_INFO,
                     reinterpret_cast<thread_info_t>(&extended_info),
                     &count);
    if (kr == KERN_SUCCESS) {
      WritePropertyBytes(
          writer,
          IntermediateDumpKey::kThreadName,
          reinterpret_cast<const void*>(extended_info.pth_name),
          strnlen(extended_info.pth_name, sizeof(extended_info.pth_name)));
    } else {
      CRASHPAD_RAW_LOG_ERROR(kr, "thread_info::THREAD_EXTENDED_INFO");
    }

    thread_precedence_policy precedence;
    count = THREAD_PRECEDENCE_POLICY_COUNT;
    boolean_t get_default = FALSE;
    kr = thread_policy_get(thread,
                           THREAD_PRECEDENCE_POLICY,
                           reinterpret_cast<thread_policy_t>(&precedence),
                           &count,
                           &get_default);
    if (kr == KERN_SUCCESS) {
      WriteProperty(
          writer, IntermediateDumpKey::kPriority, &precedence.importance);
    } else {
      CRASHPAD_RAW_LOG_ERROR(kr, "thread_policy_get");
    }

    // Thread ID.
#if defined(ARCH_CPU_ARM64)
    uint64_t thread_id;
#endif
    count = THREAD_IDENTIFIER_INFO_COUNT;
    kr = thread_info(thread,
                     THREAD_IDENTIFIER_INFO,
                     reinterpret_cast<thread_info_t>(&identifier_info),
                     &count);
    if (kr == KERN_SUCCESS) {
#if defined(ARCH_CPU_ARM64)
      thread_id = identifier_info.thread_id;
#endif
      WriteProperty(
          writer, IntermediateDumpKey::kThreadID, &identifier_info.thread_id);
      WriteProperty(writer,
                    IntermediateDumpKey::kThreadDataAddress,
                    &identifier_info.thread_handle);
    } else {
      CRASHPAD_RAW_LOG_ERROR(kr, "thread_info::THREAD_IDENTIFIER_INFO");
    }

    // thread_snapshot_ios_intermediate_dump::GenerateStackMemoryFromFrames is
    // only implemented for arm64, so no x86_64 block here.
#if defined(ARCH_CPU_ARM64)
    // For uncaught NSExceptions, use the frames passed from the system rather
    // than the current thread state.
    if (num_frames > 0 && exception_thread_id == thread_id) {
      WriteProperty(writer,
                    IntermediateDumpKey::kThreadUncaughtNSExceptionFrames,
                    frames,
                    num_frames);
      continue;
    }
#endif

#if defined(ARCH_CPU_X86_64)
    x86_thread_state64_t thread_state;
    x86_float_state64_t float_state;
    x86_debug_state64_t debug_state;
    mach_msg_type_number_t thread_state_count = x86_THREAD_STATE64_COUNT;
    mach_msg_type_number_t float_state_count = x86_FLOAT_STATE64_COUNT;
    mach_msg_type_number_t debug_state_count = x86_DEBUG_STATE64_COUNT;
#elif defined(ARCH_CPU_ARM64)
    arm_thread_state64_t thread_state;
    arm_neon_state64_t float_state;
    arm_debug_state64_t debug_state;
    mach_msg_type_number_t thread_state_count = ARM_THREAD_STATE64_COUNT;
    mach_msg_type_number_t float_state_count = ARM_NEON_STATE64_COUNT;
    mach_msg_type_number_t debug_state_count = ARM_DEBUG_STATE64_COUNT;
#endif

    kr = thread_get_state(thread,
                          kThreadStateFlavor,
                          reinterpret_cast<thread_state_t>(&thread_state),
                          &thread_state_count);
    if (kr != KERN_SUCCESS) {
      CRASHPAD_RAW_LOG_ERROR(kr, "thread_get_state::kThreadStateFlavor");
    }
    WriteProperty(writer, IntermediateDumpKey::kThreadState, &thread_state);

    kr = thread_get_state(thread,
                          kFloatStateFlavor,
                          reinterpret_cast<thread_state_t>(&float_state),
                          &float_state_count);
    if (kr != KERN_SUCCESS) {
      CRASHPAD_RAW_LOG_ERROR(kr, "thread_get_state::kFloatStateFlavor");
    }
    WriteProperty(writer, IntermediateDumpKey::kFloatState, &float_state);

    kr = thread_get_state(thread,
                          kDebugStateFlavor,
                          reinterpret_cast<thread_state_t>(&debug_state),
                          &debug_state_count);
    if (kr != KERN_SUCCESS) {
      CRASHPAD_RAW_LOG_ERROR(kr, "thread_get_state::kDebugStateFlavor");
    }
    WriteProperty(writer, IntermediateDumpKey::kDebugState, &debug_state);

#if defined(ARCH_CPU_X86_64)
    vm_address_t stack_pointer = thread_state.__rsp;
#elif defined(ARCH_CPU_ARM64)
    vm_address_t stack_pointer = arm_thread_state64_get_sp(thread_state);
#endif

    vm_size_t stack_region_size;
    const vm_address_t stack_region_address =
        CalculateStackRegion(stack_pointer, &stack_region_size);
    WriteProperty(writer,
                  IntermediateDumpKey::kStackRegionAddress,
                  &stack_region_address);
    WritePropertyBytes(writer,
                       IntermediateDumpKey::kStackRegionData,
                       reinterpret_cast<const void*>(stack_region_address),
                       stack_region_size);

    // Grab extra memory from context.
    CaptureMemoryPointedToByThreadState(writer, thread_state);
  }
}

// static
void InProcessIntermediateDumpHandler::WriteModuleInfo(
    IOSIntermediateDumpWriter* writer) {
#ifndef ARCH_CPU_64_BITS
#error Only 64-bit Mach-O is supported
#endif

  IOSIntermediateDumpWriter::ScopedArray module_array(
      writer, IntermediateDumpKey::kModules);

  task_dyld_info_data_t dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
  kern_return_t kr = task_info(mach_task_self(),
                               TASK_DYLD_INFO,
                               reinterpret_cast<task_info_t>(&dyld_info),
                               &count);
  if (kr != KERN_SUCCESS) {
    CRASHPAD_RAW_LOG_ERROR(kr, "task_info");
  }

  ScopedVMRead<dyld_all_image_infos> image_infos;
  if (!image_infos.Read(dyld_info.all_image_info_addr)) {
    CRASHPAD_RAW_LOG("Unable to dyld_info.all_image_info_addr");
    return;
  }

  uint32_t image_count = image_infos->infoArrayCount;
  const dyld_image_info* image_array = image_infos->infoArray;
  for (int32_t image_index = image_count - 1; image_index >= 0; --image_index) {
    IOSIntermediateDumpWriter::ScopedArrayMap modules(writer);
    ScopedVMRead<dyld_image_info> image;
    if (!image.Read(&image_array[image_index])) {
      CRASHPAD_RAW_LOG("Unable to dyld_image_info");
      continue;
    }

    if (image->imageFilePath) {
      WritePropertyCString(
          writer, IntermediateDumpKey::kName, PATH_MAX, image->imageFilePath);
    }
    uint64_t address = FromPointerCast<uint64_t>(image->imageLoadAddress);
    WriteProperty(writer, IntermediateDumpKey::kAddress, &address);
    WriteProperty(
        writer, IntermediateDumpKey::kTimestamp, &image->imageFileModDate);
    WriteModuleInfoAtAddress(writer, address, false /*is_dyld=false*/);
  }

  {
    IOSIntermediateDumpWriter::ScopedArrayMap modules(writer);
    if (image_infos->dyldPath) {
      WritePropertyCString(
          writer, IntermediateDumpKey::kName, PATH_MAX, image_infos->dyldPath);
    }
    uint64_t address =
        FromPointerCast<uint64_t>(image_infos->dyldImageLoadAddress);
    WriteProperty(writer, IntermediateDumpKey::kAddress, &address);
    WriteModuleInfoAtAddress(writer, address, true /*is_dyld=true*/);
  }
}

// static
void InProcessIntermediateDumpHandler::WriteExceptionFromSignal(
    IOSIntermediateDumpWriter* writer,
    const IOSSystemDataCollector& system_data,
    siginfo_t* siginfo,
    ucontext_t* context) {
  IOSIntermediateDumpWriter::ScopedMap signal_exception_map(
      writer, IntermediateDumpKey::kSignalException);

  WriteProperty(writer, IntermediateDumpKey::kSignalNumber, &siginfo->si_signo);
  WriteProperty(writer, IntermediateDumpKey::kSignalCode, &siginfo->si_code);
  WriteProperty(writer, IntermediateDumpKey::kSignalAddress, &siginfo->si_addr);

#if defined(ARCH_CPU_X86_64)
  x86_thread_state64_t thread_state = context->uc_mcontext->__ss;
  x86_float_state64_t float_state = context->uc_mcontext->__fs;
#elif defined(ARCH_CPU_ARM64)
  arm_thread_state64_t thread_state = context->uc_mcontext->__ss;
  arm_neon_state64_t float_state = context->uc_mcontext->__ns;
#else
#error Port to your CPU architecture
#endif
  WriteProperty(writer, IntermediateDumpKey::kThreadState, &thread_state);
  WriteProperty(writer, IntermediateDumpKey::kFloatState, &float_state);
  CaptureMemoryPointedToByThreadState(writer, thread_state);

  // Thread ID.
  thread_identifier_info identifier_info;
  mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
  kern_return_t kr =
      thread_info(mach_thread_self(),
                  THREAD_IDENTIFIER_INFO,
                  reinterpret_cast<thread_info_t>(&identifier_info),
                  &count);
  if (kr == KERN_SUCCESS) {
    WriteProperty(
        writer, IntermediateDumpKey::kThreadID, &identifier_info.thread_id);
  } else {
    CRASHPAD_RAW_LOG_ERROR(kr, "thread_info::self");
  }
}

// static
void InProcessIntermediateDumpHandler::WriteExceptionFromMachException(
    IOSIntermediateDumpWriter* writer,
    exception_behavior_t behavior,
    thread_t exception_thread,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t flavor,
    ConstThreadState state,
    mach_msg_type_number_t state_count) {
  IOSIntermediateDumpWriter::ScopedMap mach_exception_map(
      writer, IntermediateDumpKey::kMachException);

  WriteProperty(writer, IntermediateDumpKey::kException, &exception);
  WriteProperty(writer, IntermediateDumpKey::kCodes, code, code_count);
  WriteProperty(writer, IntermediateDumpKey::kFlavor, &flavor);
  WritePropertyBytes(writer,
                     IntermediateDumpKey::kState,
                     state,
                     state_count * sizeof(uint32_t));

  thread_identifier_info identifier_info;
  mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
  kern_return_t kr =
      thread_info(exception_thread,
                  THREAD_IDENTIFIER_INFO,
                  reinterpret_cast<thread_info_t>(&identifier_info),
                  &count);
  if (kr == KERN_SUCCESS) {
    WriteProperty(
        writer, IntermediateDumpKey::kThreadID, &identifier_info.thread_id);
  } else {
    CRASHPAD_RAW_LOG_ERROR(kr, "thread_info");
  }
}

// static
void InProcessIntermediateDumpHandler::WriteExceptionFromNSException(
    IOSIntermediateDumpWriter* writer) {
  IOSIntermediateDumpWriter::ScopedMap nsexception_map(
      writer, IntermediateDumpKey::kNSException);

  thread_identifier_info identifier_info;
  mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
  kern_return_t kr =
      thread_info(mach_thread_self(),
                  THREAD_IDENTIFIER_INFO,
                  reinterpret_cast<thread_info_t>(&identifier_info),
                  &count);
  if (kr == KERN_SUCCESS) {
    WriteProperty(
        writer, IntermediateDumpKey::kThreadID, &identifier_info.thread_id);
  } else {
    CRASHPAD_RAW_LOG_ERROR(kr, "thread_info::self");
  }
}

void InProcessIntermediateDumpHandler::WriteModuleInfoAtAddress(
    IOSIntermediateDumpWriter* writer,
    uint64_t address,
    bool is_dyld) {
  ScopedVMRead<mach_header_64> header;
  if (!header.Read(address) || header->magic != MH_MAGIC_64) {
    CRASHPAD_RAW_LOG("Invalid module header");
    return;
  }

  const load_command* unsafe_command_ptr =
      reinterpret_cast<const load_command*>(
          reinterpret_cast<const mach_header_64*>(address) + 1);

  // Rather than using an individual ScopedVMRead for each load_command, load
  // the entire block of commands at once.
  ScopedVMRead<char> all_commands;
  if (!all_commands.Read(unsafe_command_ptr, header->sizeofcmds)) {
    CRASHPAD_RAW_LOG("Unable to read module load_commands.");
    return;
  }

  // All the *_vm_read_ptr variables in the load_command loop below have been
  // vm_read in `all_commands` above, and may be dereferenced without additional
  // ScopedVMReads.
  const load_command* command_vm_read_ptr =
      reinterpret_cast<const load_command*>(all_commands.get());

  // Make sure that the basic load command structure doesn’t overflow the
  // space allotted for load commands, as well as iterating through ncmds.
  vm_size_t slide = 0;
  for (uint32_t cmd_index = 0, cumulative_cmd_size = 0;
       cmd_index < header->ncmds && cumulative_cmd_size < header->sizeofcmds;
       ++cmd_index) {
    if (command_vm_read_ptr->cmd == LC_SEGMENT_64) {
      const segment_command_64* segment_vm_read_ptr =
          reinterpret_cast<const segment_command_64*>(command_vm_read_ptr);
      if (strcmp(segment_vm_read_ptr->segname, SEG_TEXT) == 0) {
        WriteProperty(
            writer, IntermediateDumpKey::kSize, &segment_vm_read_ptr->vmsize);
        slide = address - segment_vm_read_ptr->vmaddr;
      } else if (strcmp(segment_vm_read_ptr->segname, SEG_DATA) == 0) {
        WriteDataSegmentAnnotations(writer, segment_vm_read_ptr, slide);
      }
    } else if (command_vm_read_ptr->cmd == LC_ID_DYLIB) {
      const dylib_command* dylib_vm_read_ptr =
          reinterpret_cast<const dylib_command*>(command_vm_read_ptr);
      WriteProperty(writer,
                    IntermediateDumpKey::kDylibCurrentVersion,
                    &dylib_vm_read_ptr->dylib.current_version);
    } else if (command_vm_read_ptr->cmd == LC_SOURCE_VERSION) {
      const source_version_command* source_version_vm_read_ptr =
          reinterpret_cast<const source_version_command*>(command_vm_read_ptr);
      WriteProperty(writer,
                    IntermediateDumpKey::kSourceVersion,
                    &source_version_vm_read_ptr->version);
    } else if (command_vm_read_ptr->cmd == LC_UUID) {
      const uuid_command* uuid_vm_read_ptr =
          reinterpret_cast<const uuid_command*>(command_vm_read_ptr);
      WriteProperty(
          writer, IntermediateDumpKey::kUUID, &uuid_vm_read_ptr->uuid);
    }

    cumulative_cmd_size += command_vm_read_ptr->cmdsize;
    command_vm_read_ptr = reinterpret_cast<const load_command*>(
        reinterpret_cast<const uint8_t*>(command_vm_read_ptr) +
        command_vm_read_ptr->cmdsize);
  }

  WriteProperty(writer, IntermediateDumpKey::kFileType, &header->filetype);
}

void InProcessIntermediateDumpHandler::WriteDataSegmentAnnotations(
    IOSIntermediateDumpWriter* writer,
    const segment_command_64* segment_vm_read_ptr,
    vm_size_t slide) {
  const section_64* section_vm_read_ptr = reinterpret_cast<const section_64*>(
      reinterpret_cast<uint64_t>(segment_vm_read_ptr) +
      sizeof(segment_command_64));
  for (uint32_t sect_index = 0; sect_index <= segment_vm_read_ptr->nsects;
       ++sect_index) {
    if (strcmp(section_vm_read_ptr->sectname, "crashpad_info") == 0) {
      ScopedVMRead<CrashpadInfo> crashpad_info;
      if (crashpad_info.Read(section_vm_read_ptr->addr + slide) &&
          crashpad_info->size() == sizeof(CrashpadInfo) &&
          crashpad_info->signature() == CrashpadInfo::kSignature &&
          crashpad_info->version() == 1) {
        WriteCrashpadAnnotationsList(writer, crashpad_info.get());
        WriteCrashpadSimpleAnnotationsDictionary(writer, crashpad_info.get());
      }
    } else if (strcmp(section_vm_read_ptr->sectname, "__crash_info") == 0) {
      ScopedVMRead<crashreporter_annotations_t> crash_info;
      if (!crash_info.Read(section_vm_read_ptr->addr + slide) ||
          (crash_info->version != 4 && crash_info->version != 5)) {
        continue;
      }
      WriteAppleCrashReporterAnnotations(writer, crash_info.get());
    }
    section_vm_read_ptr = reinterpret_cast<const section_64*>(
        reinterpret_cast<uint64_t>(section_vm_read_ptr) + sizeof(section_64));
  }
}

void InProcessIntermediateDumpHandler::WriteCrashpadAnnotationsList(
    IOSIntermediateDumpWriter* writer,
    CrashpadInfo* crashpad_info) {
  if (!crashpad_info->annotations_list()) {
    return;
  }
  ScopedVMRead<AnnotationList> annotation_list;
  if (!annotation_list.Read(crashpad_info->annotations_list())) {
    CRASHPAD_RAW_LOG("Unable to read annotations list object");
    return;
  }

  IOSIntermediateDumpWriter::ScopedArray annotations_array(
      writer, IntermediateDumpKey::kAnnotationObjects);
  ScopedVMRead<Annotation> current;

  // Use vm_read() to ensure that the linked-list AnnotationList head (which is
  // a dummy node of type kInvalid) is valid and copy its memory into a
  // newly-allocated buffer.
  //
  // In the case where the pointer has been clobbered or the memory range is not
  // readable, skip reading all the Annotations.
  if (!current.Read(annotation_list->head())) {
    CRASHPAD_RAW_LOG("Unable to read annotation");
    return;
  }

  for (size_t index = 0;
       current->link_node() != annotation_list.get()->tail_pointer() &&
       index < kMaxNumberOfAnnotations;
       ++index) {
    ScopedVMRead<Annotation> node;

    // Like above, use vm_read() to ensure that the node in the linked list is
    // valid and copy its memory into a newly-allocated buffer.
    //
    // In the case where the pointer has been clobbered or the memory range is
    // not readable, skip reading this and all further Annotations.
    if (!node.Read(current->link_node())) {
      CRASHPAD_RAW_LOG("Unable to read annotation");
      return;
    }
    current.Read(current->link_node());

    if (node->size() == 0)
      continue;

    if (node->size() > Annotation::kValueMaxSize) {
      CRASHPAD_RAW_LOG("Incorrect annotation length");
      continue;
    }

    // For Annotations which support guarding reads from concurrent writes, map
    // their memory read-write using vm_remap(), then declare a ScopedSpinGuard
    // which lives for the duration of the read.
    ScopedVMMap<Annotation> mapped_node;
    std::optional<ScopedSpinGuard> annotation_guard;
    if (node->concurrent_access_guard_mode() ==
        Annotation::ConcurrentAccessGuardMode::kScopedSpinGuard) {
      constexpr vm_prot_t kDesiredProtection = VM_PROT_WRITE | VM_PROT_READ;
      if (!mapped_node.Map(node.get()) ||
          (mapped_node.CurrentProtection() & kDesiredProtection) !=
              kDesiredProtection) {
        CRASHPAD_RAW_LOG("Unable to map annotation");

        // Skip this annotation rather than giving up entirely, since the linked
        // node should still be valid.
        continue;
      }

      // TODO(https://crbug.com/crashpad/438): Pass down a `params` object into
      // this method to optionally enable a timeout here.
      constexpr uint64_t kTimeoutNanoseconds = 0;
      annotation_guard =
          mapped_node->TryCreateScopedSpinGuard(kTimeoutNanoseconds);
      if (!annotation_guard) {
        // This is expected if the process is writing to the Annotation, so
        // don't log here and skip the annotation.
        continue;
      }
    }

    IOSIntermediateDumpWriter::ScopedArrayMap annotation_map(writer);
    WritePropertyCString(writer,
                         IntermediateDumpKey::kAnnotationName,
                         Annotation::kNameMaxLength,
                         reinterpret_cast<const char*>(node->name()));
    WritePropertyBytes(writer,
                       IntermediateDumpKey::kAnnotationValue,
                       reinterpret_cast<const void*>(node->value()),
                       node->size());
    Annotation::Type type = node->type();
    WritePropertyBytes(writer,
                       IntermediateDumpKey::kAnnotationType,
                       reinterpret_cast<const void*>(&type),
                       sizeof(type));
  }
}

}  // namespace internal
}  // namespace crashpad
