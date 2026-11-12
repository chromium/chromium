/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_LIBARTBASE_BASE_MEMORY_TOOL_H_
#define ART_LIBARTBASE_BASE_MEMORY_TOOL_H_

#include <stddef.h>

namespace art {

#if !defined(__has_feature)
# define __has_feature(x) 0
#endif

#if __has_feature(address_sanitizer)

# include <sanitizer/asan_interface.h>

# ifdef ART_ENABLE_ADDRESS_SANITIZER
#  define MEMORY_TOOL_MAKE_NOACCESS(p, s) __asan_poison_memory_region(p, s)
#  define MEMORY_TOOL_MAKE_UNDEFINED(p, s) __asan_unpoison_memory_region(p, s)
#  define MEMORY_TOOL_MAKE_DEFINED(p, s) __asan_unpoison_memory_region(p, s)
constexpr bool kMemoryToolIsAvailable = true;
# else
#  define MEMORY_TOOL_MAKE_NOACCESS(p, s) do { (void)(p); (void)(s); } while (0)
#  define MEMORY_TOOL_MAKE_UNDEFINED(p, s) do { (void)(p); (void)(s); } while (0)
#  define MEMORY_TOOL_MAKE_DEFINED(p, s) do { (void)(p); (void)(s); } while (0)
constexpr bool kMemoryToolIsAvailable = false;
# endif

extern "C" void __asan_handle_no_return();

# define ATTRIBUTE_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address, noinline))
# define MEMORY_TOOL_HANDLE_NO_RETURN __asan_handle_no_return()
constexpr bool kRunningOnMemoryTool = true;
constexpr bool kMemoryToolDetectsLeaks = true;
constexpr bool kMemoryToolAddsRedzones = true;
constexpr size_t kMemoryToolStackGuardSizeScale = 2;

#else

# define MEMORY_TOOL_MAKE_NOACCESS(p, s) do { (void)(p); (void)(s); } while (0)
# define MEMORY_TOOL_MAKE_UNDEFINED(p, s) do { (void)(p); (void)(s); } while (0)
# define MEMORY_TOOL_MAKE_DEFINED(p, s) do { (void)(p); (void)(s); } while (0)
# define ATTRIBUTE_NO_SANITIZE_ADDRESS
# define MEMORY_TOOL_HANDLE_NO_RETURN do { } while (0)
constexpr bool kRunningOnMemoryTool = false;
constexpr bool kMemoryToolIsAvailable = false;
constexpr bool kMemoryToolDetectsLeaks = false;
constexpr bool kMemoryToolAddsRedzones = false;
constexpr size_t kMemoryToolStackGuardSizeScale = 1;

#endif

#if __has_feature(hwaddress_sanitizer)
# define HWADDRESS_SANITIZER
// NB: The attribute also implies NO_INLINE. If inlined, the hwasan attribute would be lost.
//     If method is also separately marked as ALWAYS_INLINE, the NO_INLINE takes precedence.
# define ATTRIBUTE_NO_SANITIZE_HWADDRESS __attribute__((no_sanitize("hwaddress"), noinline))
#else
# define ATTRIBUTE_NO_SANITIZE_HWADDRESS
#endif

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_MEMORY_TOOL_H_
