// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MALDOCA_BASE_SOURCE_LOCATION_H_
#define MALDOCA_BASE_SOURCE_LOCATION_H_

#ifdef MALDOCA_CHROME
#include <cstdint>

namespace maldoca {
class SourceLocation {
 public:
  constexpr SourceLocation(std::uint_least32_t l = 0, const char* f = nullptr)
      : line_(l), file_(f) {}

  SourceLocation(const SourceLocation& other) = default;
  SourceLocation& operator=(const SourceLocation& other) = default;

  inline std::uint_least32_t line() const { return line_; }
  inline const char* file_name() const { return file_; }

  static constexpr SourceLocation GetSourceLocation(const std::uint_least32_t l,
                                                    const char* f) {
    return SourceLocation(l, f);
  }

 private:
  std::uint_least32_t line_;
  const char* file_;
};

}  // namespace maldoca

#define MALDOCA_LOC \
  ::maldoca::SourceLocation::GetSourceLocation(__LINE__, __FILE__)

#else
#include "zetasql/base/source_location.h"
namespace maldoca {
using ::zetasql_base::SourceLocation;
}  // namespace maldoca
#define MALDOCA_LOC ZETASQL_LOC

#endif  // MALDOCA_CHROME
#endif  // MALDOCA_BASE_SOURCE_LOCATION_H_
