/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow_lite_support/c/common_utils.h"

#include <string>

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/cord.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"

namespace tflite {
namespace support {

void CreateTfLiteSupportError(enum TfLiteSupportErrorCode code,
                              const char* message, TfLiteSupportError** error) {
  if (error == nullptr) return;

  *error = new TfLiteSupportError;
  (*error)->code = code;
  (*error)->message = strdup(message);
}

void CreateTfLiteSupportErrorWithStatus(const absl::Status& status,
                                        TfLiteSupportError** error) {
  if (status.ok() || error == nullptr) return;

  // Payload of absl::Status created by the tflite task library stores an
  // appropriate value of the enum TfLiteSupportStatus. The integer value
  // corresponding to the TfLiteSupportStatus enum stored in the payload is
  // extracted here to later map to the appropriate error code to be returned.
  // In cases where the enum is not stored in (payload is NULL or the payload
  // string cannot be converted to an integer), we set the error code value to
  // be 1 (kError of TfLiteErrorCode used in the C library to signify any errors
  // not falling into other categories.) Since payload is of type absl::Cord
  // that can be type cast into an absl::optional<std::string>, we use the
  // std::stoi function to convert it into an integer code if possible.
  int generic_error_code = static_cast<int>(kError);
  int error_code;
  try {
    // Try converting payload to integer if payload is not empty. Otherwise
    // convert a string signifying generic error code kError to integer.
    error_code = std::stoi(static_cast<absl::optional<std::string>>(
                               status.GetPayload(kTfLiteSupportPayload))
                               .value_or(std::to_string(generic_error_code)));
  } catch (std::invalid_argument& e) {
    // If non empty payload string cannot be converted to an integer. Set error
    // code to 1(kError).
    error_code = generic_error_code;
  }

  // If error_code is outside the range of enum values possible or is kError, we
  // try to map the absl::Status::code() to assign appropriate
  // TfLiteSupportErrorCode or kError in default cases. Note: The mapping to
  // absl::Status::code() is done to generate a more specific error code than
  // kError in cases when the payload can't be mapped to TfLiteSupportStatus.
  // This can happen when absl::Status returned by TfLite are in turn returned
  // without moodification by TfLite Support Methods.
  if (error_code > static_cast<int>(kErrorCodeLast) ||
      error_code <= static_cast<int>(kErrorCodeFirst)) {
    switch (status.code()) {
      case absl::StatusCode::kInternal:
        error_code = kInternalError;
        break;
      case absl::StatusCode::kInvalidArgument:
        error_code = kInvalidArgumentError;
        break;
      case absl::StatusCode::kNotFound:
        error_code = kNotFoundError;
        break;
      default:
        error_code = kError;
        break;
    }
  }

  // Creates the TfLiteSupportError with the appropriate error
  // TfLiteSupportErrorCode and message. TfLiteErrorCode has a one to one
  // mapping with TfLiteSupportStatus starting from the value 1(kError) and
  // hence will be correctly initialized if directly cast from the integer code
  // derived from TfLiteSupportStatus stored in payload. TfLiteErrorCode omits
  // kOk = 0 of TfLiteSupportStatus.
  //
  // Stores a string including absl status code and message(if non empty) as the
  // error message See
  // https://github.com/abseil/abseil-cpp/blob/master/absl/status/status.h#L514
  // for explanation. absl::Status::message() can also be used but not always
  // guaranteed to be non empty.
  CreateTfLiteSupportError(
      static_cast<TfLiteSupportErrorCode>(error_code),
      status.ToString(absl::StatusToStringMode::kWithNoExtraData).c_str(),
      error);
}

}  // namespace support
}  // namespace tflite
