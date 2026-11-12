/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_LIBDEXFILE_DEX_DEX_FILE_TRACKING_REGISTRAR_H_
#define ART_LIBDEXFILE_DEX_DEX_FILE_TRACKING_REGISTRAR_H_

#include <deque>
#include <tuple>

#include "dex_file.h"

namespace art {
namespace dex {
namespace tracking {

// Class for (un)poisoning various sections of Dex Files
//
// This class provides the means to log accesses only of sections whose
// accesses are needed. All accesses are displayed as stack traces in
// logcat.
class DexFileTrackingRegistrar {
 public:
  explicit DexFileTrackingRegistrar(const DexFile* const dex_file)
      : dex_file_(dex_file) {
  }

  // This function is where the functions below it are called to actually
  // poison sections.
  void SetDexSections();

  // Uses data contained inside range_values_ to poison memory through the
  // memory tool.
  void SetCurrentRanges();

 private:
  void SetDexFileRegistration(bool should_poison);

  // Set of functions concerning Code Items of dex_file_
  void SetAllCodeItemRegistration(bool should_poison);
  // Sets the insns_ section of all code items.
  void SetAllInsnsRegistration(bool should_poison);
  // This function finds the code item of a class based on class name.
  void SetCodeItemRegistration(const char* class_name, bool should_poison);
  // Sets the size and offset information along with first instruction in insns_
  // section of all code items.
  void SetAllCodeItemStartRegistration(bool should_poison);

  // Set of functions concerning String Data Items of dex_file_
  void SetAllStringDataRegistration(bool should_poison);
  // Sets the first byte of size value and data section of all string data
  // items.
  void SetAllStringDataStartRegistration(bool should_poison);

  // Contains tuples of all ranges of memory that need to be explicitly
  // (un)poisoned by the memory tool.
  std::deque<std::tuple<const void *, size_t, bool>> range_values_;

  const DexFile* const dex_file_;
};

// This function is meant to called externally to use DexfileTrackingRegistrar
void RegisterDexFile(const DexFile* dex_file);

}  // namespace tracking
}  // namespace dex
}  // namespace art

#endif  // ART_LIBDEXFILE_DEX_DEX_FILE_TRACKING_REGISTRAR_H_
