// Copyright 2022 The Crashpad Authors
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

#import "test/ios/host/handler_forbidden_allocators.h"

#include <CoreFoundation/CoreFoundation.h>
#include <malloc/malloc.h>
#include <pthread.h>

#include <limits>

#include "base/apple/mach_logging.h"
#include "base/check_op.h"
#include "client/crashpad_client.h"
#include "util/ios/raw_logging.h"

namespace crashpad {
namespace test {

namespace {

uint64_t g_main_thread = 0;
uint64_t g_mach_exception_thread = 0;

// Somewhat simplified logic copied from Chromium's
// base/allocator/partition_allocator/src/partition_alloc/shim/malloc_zone_functions_apple.h.
// The arrays g_original_zones and g_original_zones_ptr stores all information
// about malloc zones before they are shimmed. This information needs to be
// accessed during dispatch back into the zone.
constexpr int kMaxZoneCount = 30;
malloc_zone_t g_original_zones[kMaxZoneCount];
malloc_zone_t* g_original_zones_ptr[kMaxZoneCount];
unsigned int g_zone_count = 0;

struct _malloc_zone_t original_zone_for_zone(struct _malloc_zone_t* zone) {
  for (unsigned int i = 0; i < g_zone_count; ++i) {
    if (g_original_zones_ptr[i] == zone) {
      return g_original_zones[i];
    }
  }
  return g_original_zones[0];
}

bool is_handler_thread() {
  uint64_t thread_self;
  pthread_threadid_np(pthread_self(), &thread_self);
  return (thread_self == g_main_thread ||
          thread_self == g_mach_exception_thread);
}

void* handler_forbidden_malloc(struct _malloc_zone_t* zone, size_t size) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_malloc allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return original_zone_for_zone(zone).malloc(zone, size);
}

void* handler_forbidden_calloc(struct _malloc_zone_t* zone,
                               size_t num_items,
                               size_t size) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_calloc allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return original_zone_for_zone(zone).calloc(zone, num_items, size);
}

void* handler_forbidden_valloc(struct _malloc_zone_t* zone, size_t size) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_valloc allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return original_zone_for_zone(zone).valloc(zone, size);
}

void handler_forbidden_free(struct _malloc_zone_t* zone, void* ptr) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_free allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  original_zone_for_zone(zone).free(zone, ptr);
}

void* handler_forbidden_realloc(struct _malloc_zone_t* zone,
                                void* ptr,
                                size_t size) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_realloc allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return original_zone_for_zone(zone).realloc(zone, ptr, size);
}

void handler_forbidden_destroy(struct _malloc_zone_t* zone) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_destroy allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  original_zone_for_zone(zone).destroy(zone);
}

void* handler_forbidden_memalign(struct _malloc_zone_t* zone,
                                 size_t alignment,
                                 size_t size) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_memalign allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return original_zone_for_zone(zone).memalign(zone, alignment, size);
}

unsigned handler_forbidden_batch_malloc(struct _malloc_zone_t* zone,
                                        size_t size,
                                        void** results,
                                        unsigned num_requested) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG(
        "handler_forbidden_batch_malloc allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return original_zone_for_zone(zone).batch_malloc(
      zone, size, results, num_requested);
}

void handler_forbidden_batch_free(struct _malloc_zone_t* zone,
                                  void** to_be_freed,
                                  unsigned num_to_be_freed) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_batch_free allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  original_zone_for_zone(zone).batch_free(zone, to_be_freed, num_to_be_freed);
}

void handler_forbidden_free_definite_size(struct _malloc_zone_t* zone,
                                          void* ptr,
                                          size_t size) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG(
        "handler_forbidden_free_definite_size allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  original_zone_for_zone(zone).free_definite_size(zone, ptr, size);
}

size_t handler_forbidden_pressure_relief(struct _malloc_zone_t* zone,
                                         size_t goal) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG(
        "handler_forbidden_pressure_relief allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return original_zone_for_zone(zone).pressure_relief(zone, goal);
}

boolean_t handler_forbidden_claimed_address(struct _malloc_zone_t* zone,
                                            void* ptr) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG(
        "handler_forbidden_claimed_address allocator used in handler.");
    exit(EXIT_FAILURE);
  }

  if (original_zone_for_zone(zone).claimed_address) {
    return original_zone_for_zone(zone).claimed_address(zone, ptr);
  }

  // If the fast API 'claimed_address' is not implemented in the specified zone,
  // fall back to 'size' function, which also tells whether the given address
  // belongs to the zone or not although it'd be slow.
  return original_zone_for_zone(zone).size(zone, ptr);
}

#if defined(__IPHONE_16_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_1
// The fallback function to be called when try_free_default_function receives a
// pointer which doesn't belong to the allocator.
void TryFreeDefaultFallbackToFindZoneAndFree(void* ptr) {
  unsigned int zone_count = 0;
  vm_address_t* zones = nullptr;
  kern_return_t result =
      malloc_get_all_zones(mach_task_self(), nullptr, &zones, &zone_count);
  MACH_CHECK(result == KERN_SUCCESS, result) << "malloc_get_all_zones";

  // "find_zone_and_free" expected by try_free_default.
  //
  // libmalloc's zones call find_registered_zone() in case the default one
  // doesn't handle the allocation. We can't, so we try to emulate it. See the
  // implementation in libmalloc/src/malloc.c for details.
  // https://github.com/apple-oss-distributions/libmalloc/blob/main/src/malloc.c
  for (unsigned int i = 0; i < zone_count; ++i) {
    malloc_zone_t* zone = reinterpret_cast<malloc_zone_t*>(zones[i]);
    if (size_t size = zone->size(zone, ptr)) {
      if (zone->version >= 6 && zone->free_definite_size) {
        zone->free_definite_size(zone, ptr, size);
      } else {
        zone->free(zone, ptr);
      }
      return;
    }
  }
}

void handler_forbidden_try_free_default(struct _malloc_zone_t* zone,
                                        void* ptr) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG(
        "handler_forbidden_try_free_default allocator used in handler.");
    exit(EXIT_FAILURE);
  }

  if (original_zone_for_zone(zone).try_free_default) {
    return original_zone_for_zone(zone).try_free_default(zone, ptr);
  }
  TryFreeDefaultFallbackToFindZoneAndFree(ptr);
}
#endif

size_t handler_forbidden_size(struct _malloc_zone_t* zone, const void* ptr) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_size allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return original_zone_for_zone(zone).size(zone, ptr);
}

bool DeprotectMallocZone(malloc_zone_t* default_zone,
                         vm_address_t* reprotection_start,
                         vm_size_t* reprotection_length,
                         vm_prot_t* reprotection_value) {
  mach_port_t unused;
  *reprotection_start = reinterpret_cast<vm_address_t>(default_zone);
  struct vm_region_basic_info_64 info;
  mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
  kern_return_t result = vm_region_64(mach_task_self(),
                                      reprotection_start,
                                      reprotection_length,
                                      VM_REGION_BASIC_INFO_64,
                                      reinterpret_cast<vm_region_info_t>(&info),
                                      &count,
                                      &unused);
  if (result != KERN_SUCCESS) {
    MACH_LOG(ERROR, result) << "vm_region_64";
    return false;
  }

  // The kernel always returns a null object for VM_REGION_BASIC_INFO_64, but
  // balance it with a deallocate in case this ever changes. See
  // the VM_REGION_BASIC_INFO_64 case in vm_map_region() in 10.15's
  // https://opensource.apple.com/source/xnu/xnu-6153.11.26/osfmk/vm/vm_map.c .
  mach_port_deallocate(mach_task_self(), unused);

  if (!(info.max_protection & VM_PROT_WRITE)) {
    LOG(ERROR) << "Invalid max_protection " << info.max_protection;
    return false;
  }

  // Does the region fully enclose the zone pointers? Possibly unwarranted
  // simplification used: using the size of a full version 10 malloc zone rather
  // than the actual smaller size if the passed-in zone is not version 10.
  DCHECK_LE(*reprotection_start, reinterpret_cast<vm_address_t>(default_zone));
  vm_size_t zone_offset = reinterpret_cast<vm_address_t>(default_zone) -
                          reinterpret_cast<vm_address_t>(*reprotection_start);
  DCHECK_LE(zone_offset + sizeof(malloc_zone_t), *reprotection_length);

  if (info.protection & VM_PROT_WRITE) {
    // No change needed; the zone is already writable.
    *reprotection_start = 0;
    *reprotection_length = 0;
    *reprotection_value = VM_PROT_NONE;
  } else {
    *reprotection_value = info.protection;
    result = vm_protect(mach_task_self(),
                        *reprotection_start,
                        *reprotection_length,
                        false,
                        info.protection | VM_PROT_WRITE);
    if (result != KERN_SUCCESS) {
      MACH_LOG(ERROR, result) << "vm_protect";
      return false;
    }
  }
  return true;
}

void ReplaceZoneFunctions(malloc_zone_t* zone, const malloc_zone_t* functions) {
  // Remove protection.
  vm_address_t reprotection_start = 0;
  vm_size_t reprotection_length = 0;
  vm_prot_t reprotection_value = VM_PROT_NONE;
  bool success = DeprotectMallocZone(
      zone, &reprotection_start, &reprotection_length, &reprotection_value);
  if (!success) {
    return;
  }

  zone->size = functions->size;
  zone->malloc = functions->malloc;
  zone->calloc = functions->calloc;
  zone->valloc = functions->valloc;
  zone->free = functions->free;
  zone->realloc = functions->realloc;
  zone->destroy = functions->destroy;
  zone->batch_malloc = functions->batch_malloc;
  zone->batch_free = functions->batch_free;
  zone->memalign = functions->memalign;
  zone->free_definite_size = functions->free_definite_size;
  zone->pressure_relief = functions->pressure_relief;
  zone->claimed_address = functions->claimed_address;
#if defined(__IPHONE_16_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_1
  if (zone->version >= 13 && functions->try_free_default) {
    zone->try_free_default = functions->try_free_default;
  }
#endif

  // Restore protection if it was active.
  if (reprotection_start) {
    kern_return_t result = vm_protect(mach_task_self(),
                                      reprotection_start,
                                      reprotection_length,
                                      false,
                                      reprotection_value);
    if (result != KERN_SUCCESS) {
      MACH_LOG(ERROR, result) << "vm_protect";
      return;
    }
  }
}

}  // namespace

void ReplaceAllocatorsWithHandlerForbidden() {
  pthread_threadid_np(pthread_self(), &g_main_thread);

  CrashpadClient crashpad_client;
  g_mach_exception_thread = crashpad_client.GetThreadIdForTesting();

  malloc_zone_t new_functions = {};
  new_functions.size = handler_forbidden_size;
  new_functions.malloc = handler_forbidden_malloc;
  new_functions.calloc = handler_forbidden_calloc;
  new_functions.valloc = handler_forbidden_valloc;
  new_functions.free = handler_forbidden_free;
  new_functions.realloc = handler_forbidden_realloc;
  new_functions.destroy = handler_forbidden_destroy;
  new_functions.batch_malloc = handler_forbidden_batch_malloc;
  new_functions.batch_free = handler_forbidden_batch_free;
  new_functions.memalign = handler_forbidden_memalign;
  new_functions.free_definite_size = handler_forbidden_free_definite_size;
  new_functions.pressure_relief = handler_forbidden_pressure_relief;
  new_functions.claimed_address = handler_forbidden_claimed_address;
#if defined(__IPHONE_16_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_1
  new_functions.try_free_default = handler_forbidden_try_free_default;
#endif
  malloc_zone_t* default_zone = malloc_default_zone();
  g_original_zones_ptr[g_zone_count] = default_zone;
  ReplaceZoneFunctions(&g_original_zones[g_zone_count++], default_zone);
  ReplaceZoneFunctions(default_zone, &new_functions);

  malloc_zone_t* purgeable_zone = malloc_default_purgeable_zone();
  g_original_zones_ptr[g_zone_count] = purgeable_zone;
  ReplaceZoneFunctions(&g_original_zones[g_zone_count++], purgeable_zone);
  ReplaceZoneFunctions(purgeable_zone, &new_functions);

  vm_address_t* zones;
  unsigned int count;
  kern_return_t kr =
      malloc_get_all_zones(mach_task_self(), nullptr, &zones, &count);
  if (kr != KERN_SUCCESS)
    return;
  for (unsigned int i = 0; i < count; ++i) {
    malloc_zone_t* zone = reinterpret_cast<malloc_zone_t*>(zones[i]);
    g_original_zones_ptr[g_zone_count] = zone;
    ReplaceZoneFunctions(&g_original_zones[g_zone_count++], zone);
    ReplaceZoneFunctions(zone, &new_functions);

    if (g_zone_count >= kMaxZoneCount)
      break;
  }
}

}  // namespace test
}  // namespace crashpad
