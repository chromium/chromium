/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <cxxabi.h>
#include <stdlib.h>

#include <array>
#include <string>

#include <rustc_demangle.h>

#include <unwindstack/Demangle.h>

namespace unwindstack {

std::string DemangleNameIfNeeded(const std::string& name) {
  if (name.length() < 2 || name[0] != '_') {
    return name;
  }

  if (name[1] == 'Z') {
    // Try to demangle C++ name.
    char* demangled_str =
        abi::__cxa_demangle(name.c_str(), nullptr, nullptr, nullptr);
    return demangled_str != nullptr ? std::string(demangled_str) : name;
  }

  if (name[1] == 'R') {
    // Try to demangle rust name.
    // This uses the same buffer size as:
    // https://source.chromium.org/chromium/chromium/src/+/main:third_party/breakpad/breakpad/src/common/language.cc;l=186;drc=f5123d71965578abf905d6388a79a232597bb1eb
    std::array<char, 1 * 1024 * 1024> rustc_demangled;
    int success = rustc_demangle(name.c_str(), rustc_demangled.data(),
                                 rustc_demangled.size());
    return success == 1 ? std::string(rustc_demangled.data()) : name;
  }

  return name;
}

}  // namespace unwindstack
