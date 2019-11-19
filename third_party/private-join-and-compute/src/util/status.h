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

#ifndef UTIL_STATUS_H_
#define UTIL_STATUS_H_

#include <string>

namespace private_join_and_compute {
enum StatusCode {
  kOk = 0,
  kCancelled = 1,
  kUnknown = 2,
  kInvalidArgument = 3,
  kDeadlineExceeded = 4,
  kNotFound = 5,
  kAlreadyExists = 6,
  kPermissionDenied = 7,
  kResourceExhausted = 8,
  kFailedPrecondition = 9,
  kAborted = 10,
  kOutOfRange = 11,
  kUnimplemented = 12,
  kInternal = 13,
  kUnavailable = 14,
  kDataLoss = 15,
  kUnauthenticated = 16,
  kDoNotUseReservedForFutureExpansionUseDefaultInSwitchInstead_ = 20
};

// A Status is a combination of an error code and a string message (for non-OK
// error codes).
class Status {
 public:
  // Creates an OK status
  Status();

  // Make a Status from the specified error and message.
  Status(private_join_and_compute::StatusCode error, std::string error_message);

  Status(const Status& other);
  Status& operator=(const Status& other);

  // Some pre-defined Status objects
  static Status OK() {
    return Status();
  }
  static Status UNKNOWN() {
    return Status(private_join_and_compute::StatusCode::kUnknown, "");
  }

  // Accessors
  bool ok() const { return code_ == private_join_and_compute::StatusCode::kOk; }
  int error_code() const { return code_; }
  private_join_and_compute::StatusCode code() const { return code_; }
  const std::string& message() const { return message_; }

  bool operator==(const Status& x) const;
  bool operator!=(const Status& x) const;

  // NoOp
  void IgnoreError() const {}

  std::string ToString() const;

 private:
  private_join_and_compute::StatusCode code_;
  std::string message_;
};

inline bool Status::operator==(const Status& other) const {
  return (this->code_ == other.code_) && (this->message_ == other.message_);
}

inline bool Status::operator!=(const Status& other) const {
  return !(*this == other);
}

// Returns a Status that is identical to 's' except that the error_message()
// has been augmented by adding 'msg' to the end of the original error
// message.
Status Annotate(const Status& s, const std::string& msg);

extern std::ostream& operator<<(std::ostream& os, const Status& other);

inline Status OkStatus() { return Status(); }

}  // namespace private_join_and_compute

#endif  // UTIL_STATUS_H_
