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

#ifndef MALDOCA_BASE_PARSE_TEXT_PROTO_H_
#define MALDOCA_BASE_PARSE_TEXT_PROTO_H_

#include <string>

#ifndef MALDOCA_CHROME
#include "google/protobuf/text_format.h"
#endif  // MALDOCA_CHROME

#include "maldoca/base/logging.h"

namespace maldoca {

#ifndef MALDOCA_CHROME
template <typename T>
T ParseTextOrDie(const std::string& asciipb) {
  T result;
  CHECK(google::protobuf::TextFormat::ParseFromString(asciipb, &result));
  return result;
}

}  // namespace maldoca
#endif  // MALDOCA_CHROME

#endif  // MALDOCA_BASE_PARSE_TEXT_PROTO_H_
