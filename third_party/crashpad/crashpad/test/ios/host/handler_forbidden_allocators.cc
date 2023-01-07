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

#include "base/mac/mach_logging.h"
#include "client/crashpad_client.h"
#include "util/ios/raw_logging.h"

namespace crashpad {
namespace test {

namespace {

uint64_t g_main_thread = 0;
uint64_t g_mach_exception_thread = 0;
malloc_zone_t g_old_zone;

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
  return g_old_zone.malloc(zone, size);
}

void* handler_forbidden_calloc(struct _malloc_zone_t* zone,
                               size_t num_items,
                               size_t size) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_calloc allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return g_old_zone.calloc(zone, num_items, size);
}

void* handler_forbidden_valloc(struct _malloc_zone_t* zone, size_t size) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_valloc allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return g_old_zone.valloc(zone, size);
}

void handler_forbidden_free(struct _malloc_zone_t* zone, void* ptr) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_free allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  g_old_zone.free(zone, ptr);
}

void* handler_forbidden_realloc(struct _malloc_zone_t* zone,
                                void* ptr,
                                size_t size) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_realloc allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return g_old_zone.realloc(zone, ptr, size);
}

void handler_forbidden_destroy(struct _malloc_zone_t* zone) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_destroy allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  g_old_zone.destroy(zone);
}

void* handler_forbidden_memalign(struct _malloc_zone_t* zone,
                                 size_t alignment,
                                 size_t size) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_memalign allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return g_old_zone.memalign(zone, alignment, size);
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
  return g_old_zone.batch_malloc(zone, size, results, num_requested);
}

void handler_forbidden_batch_free(struct _malloc_zone_t* zone,
                                  void** to_be_freed,
                                  unsigned num_to_be_freed) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_batch_free allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  g_old_zone.batch_free(zone, to_be_freed, num_to_be_freed);
}

void handler_forbidden_free_definite_size(struct _malloc_zone_t* zone,
                                          void* ptr,
                                          size_t size) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG(
        "handler_forbidden_free_definite_size allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  g_old_zone.free_definite_size(zone, ptr, size);
}

size_t handler_forbidden_pressure_relief(struct _malloc_zone_t* zone,
                                         size_t goal) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG(
        "handler_forbidden_pressure_relief allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return g_old_zone.pressure_relief(zone, goal);
}

boolean_t handler_forbidden_claimed_address(struct _malloc_zone_t* zone,
                                            void* ptr) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG(
        "handler_forbidden_claimed_address allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return g_old_zone.claimed_address(zone, ptr);
}

size_t handler_forbidden_size(struct _malloc_zone_t* zone, const void* ptr) {
  if (is_handler_thread()) {
    CRASHPAD_RAW_LOG("handler_forbidden_size allocator used in handler.");
    exit(EXIT_FAILURE);
  }
  return g_old_zone.size(zone, ptr);
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
  zone->introspect = functions->introspect;
  zone->memalign = functions->memalign;
  zone->free_definite_size = functions->free_definite_size;
  zone->pressure_relief = functions->pressure_relief;
  zone->claimed_address = functions->claimed_address;

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

  malloc_zone_t* default_zone = malloc_default_zone();
  memcpy(&g_old_zone, default_zone, sizeof(g_old_zone));
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
  ReplaceZoneFunctions(default_zone, &new_functions);

  malloc_zone_t* purgeable_zone = malloc_default_purgeable_zone();
  ReplaceZoneFunctions(purgeable_zone, &new_functions);
}

}  // namespace test
}  // namespace crashpad
