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

#ifndef ART_LIBARTBASE_BASE_SDK_VERSION_H_
#define ART_LIBARTBASE_BASE_SDK_VERSION_H_

#include <cstdint>
#include <limits>

namespace art {

enum class SdkVersion : uint32_t {
  kMin   =  0u,
  kUnset =  0u,
  kL     = 21u,
  kL_MR1 = 22u,
  kM     = 23u,
  kN     = 24u,
  kN_MR1 = 25u,
  kO     = 26u,
  kO_MR1 = 27u,
  kP     = 28u,
  kQ     = 29u,
  kR     = 30u,
  kS     = 31u,
  kS_V2  = 32u,
  kT     = 33u,
  kMax   = std::numeric_limits<uint32_t>::max(),
};

inline bool IsSdkVersionSetAndMoreThan(uint32_t lhs, SdkVersion rhs) {
  return lhs != static_cast<uint32_t>(SdkVersion::kUnset) && lhs > static_cast<uint32_t>(rhs);
}

inline bool IsSdkVersionSetAndAtLeast(uint32_t lhs, SdkVersion rhs) {
  return lhs != static_cast<uint32_t>(SdkVersion::kUnset) && lhs >= static_cast<uint32_t>(rhs);
}

inline bool IsSdkVersionSetAndAtMost(uint32_t lhs, SdkVersion rhs) {
  return lhs != static_cast<uint32_t>(SdkVersion::kUnset) && lhs <= static_cast<uint32_t>(rhs);
}

inline bool IsSdkVersionSetAndLessThan(uint32_t lhs, SdkVersion rhs) {
  return lhs != static_cast<uint32_t>(SdkVersion::kUnset) && lhs < static_cast<uint32_t>(rhs);
}

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_SDK_VERSION_H_
