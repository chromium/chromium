/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "art_api/dex_file_support.h"

#include <dlfcn.h>
#include <inttypes.h>
#include <mutex>
#include <sys/stat.h>

#ifndef STATIC_LIB
// Not used in the static lib, so avoid a dependency on this header in
// libdexfile_support_static.
#include <log/log.h>
#endif

namespace art_api {
namespace dex {

#if defined(STATIC_LIB)
#define DEFINE_ADEX_FILE_SYMBOL(DLFUNC) decltype(DLFUNC)* g_##DLFUNC = DLFUNC;
#else
#define DEFINE_ADEX_FILE_SYMBOL(DLFUNC) decltype(DLFUNC)* g_##DLFUNC = nullptr;
#endif
FOR_EACH_ADEX_FILE_SYMBOL(DEFINE_ADEX_FILE_SYMBOL)
#undef DEFINE_ADEX_FILE_SYMBOL

bool TryLoadLibdexfile([[maybe_unused]] std::string* err_msg) {
#if defined(STATIC_LIB)
  // Nothing to do here since all function pointers are initialised statically.
  return true;
#elif defined(NO_DEXFILE_SUPPORT)
  *err_msg = "Dex file support not available.";
  return false;
#else
  // Use a plain old mutex since we want to try again if loading fails (to set
  // err_msg, if nothing else).
  static std::mutex load_mutex;
  static bool is_loaded = false;
  std::lock_guard<std::mutex> lock(load_mutex);

  if (!is_loaded) {
    // Check which version is already loaded to avoid loading both debug and
    // release builds. We might also be backtracing from separate process, in
    // which case neither is loaded.
    const char* so_name = "libdexfiled.so";
    void* handle = dlopen(so_name, RTLD_NOLOAD | RTLD_NOW | RTLD_NODELETE);
    if (handle == nullptr) {
      so_name = "libdexfile.so";
      handle = dlopen(so_name, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
    }
    if (handle == nullptr) {
      *err_msg = dlerror();
      return false;
    }

#define RESOLVE_ADEX_FILE_SYMBOL(DLFUNC) \
    auto DLFUNC##_ptr = reinterpret_cast<decltype(DLFUNC)*>(dlsym(handle, #DLFUNC)); \
    if (DLFUNC##_ptr == nullptr) { \
      *err_msg = dlerror(); \
      return false; \
    }
FOR_EACH_ADEX_FILE_SYMBOL(RESOLVE_ADEX_FILE_SYMBOL)
#undef RESOLVE_ADEX_FILE_SYMBOL

#define SET_ADEX_FILE_SYMBOL(DLFUNC) g_##DLFUNC = DLFUNC##_ptr;
    FOR_EACH_ADEX_FILE_SYMBOL(SET_ADEX_FILE_SYMBOL);
#undef SET_ADEX_FILE_SYMBOL

    is_loaded = true;
  }

  return is_loaded;
#endif  // !defined(NO_DEXFILE_SUPPORT) && !defined(STATIC_LIB)
}

void LoadLibdexfile() {
#ifndef STATIC_LIB
  if (std::string err_msg; !TryLoadLibdexfile(&err_msg)) {
    LOG_ALWAYS_FATAL("%s", err_msg.c_str());
  }
#endif
}

}  // namespace dex
}  // namespace art_api
