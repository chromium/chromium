/*
 * Copyright 2019 Google Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "third_party/private-join-and-compute/src/util/status.h"

#include <sstream>
#include <utility>

namespace private_join_and_compute {

Status::Status() : code_(private_join_and_compute::StatusCode::kOk), message_("") {}

Status::Status(private_join_and_compute::StatusCode error, std::string error_message)
    : code_(error), message_(std::move(error_message)) {
  if (code_ == private_join_and_compute::StatusCode::kOk) {
    message_.clear();
  }
}

Status::Status(const Status& other)
    : code_(other.code_), message_(other.message_) {}

Status& Status::operator=(const Status& other) {
  code_ = other.code_;
  message_ = other.message_;
  return *this;
}

std::string Status::ToString() const {
  if (code_ == private_join_and_compute::StatusCode::kOk) {
    return "OK";
  }
  std::ostringstream stringStream;
  stringStream << code_ << ": " << message_;
  return stringStream.str();
}

Status Annotate(const Status& s, const std::string& msg) {
  if (s.ok() || msg.empty()) return s;

  std::string new_msg;

  if (s.message().empty()) {
    new_msg = msg;
  } else {
    new_msg = s.message() + "; " + msg;
  }
  return Status(s.code(), new_msg);
}

extern std::ostream& operator<<(std::ostream& os, const Status& other) {
  os << other.ToString();
  return os;
}

}  // namespace private_join_and_compute
