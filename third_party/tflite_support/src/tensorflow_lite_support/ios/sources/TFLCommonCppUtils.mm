// Copyright 2022 The TensorFlow Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string>

#include "absl/status/status.h"  // from @com_google_absl  // from @com_google_absl
#include "absl/strings/cord.h"  // from @com_google_absl   // from @com_google_absl

#include "tensorflow_lite_support/cc/common.h"

#import "tensorflow_lite_support/ios/sources/TFLCommon.h"
#import "tensorflow_lite_support/ios/sources/TFLCommonCppUtils.h"

@implementation TFLCommonCppUtils

+ (BOOL)checkCError:(TfLiteSupportError *)supportError toError:(NSError **)error {
  if (!supportError) {
    return YES;
  }
  NSString *description = [NSString stringWithCString:supportError->message
                                             encoding:NSUTF8StringEncoding];
  [TFLCommonUtils createCustomError:error withCode:supportError->code description:description];
  return NO;
}

+ (BOOL)checkCppError:(const absl::Status &)status toError:(NSError *_Nullable *)error {
  if (status.ok()) {
    return YES;
  }
  // Payload of absl::Status created by the tflite task library stores an appropriate value of the
  // enum TfLiteSupportStatus. The integer value corresponding to the TfLiteSupportStatus enum
  // stored in the payload is extracted here to later map to the appropriate error code to be
  // returned. In cases where the enum is not stored in (payload is NULL or the payload string
  // cannot be converted to an integer), we set the error code value to be 1
  // (TFLSupportErrorCodeUnspecifiedError of TFLSupportErrorCode used in the iOS library to signify
  // any errors not falling into other categories.) Since payload is of type absl::Cord that can be
  // type cast into an absl::optional<std::string>, we use the std::stoi function to convert it into
  // an integer code if possible.
  NSUInteger genericErrorCode = TFLSupportErrorCodeUnspecifiedError;
  NSUInteger errorCode;
  try {
    // Try converting payload to integer if payload is not empty. Otherwise convert a string
    // signifying generic error code TFLSupportErrorCodeUnspecifiedError to integer.
    errorCode =
        (NSUInteger)std::stoi(static_cast<absl::optional<std::string>>(
                                  status.GetPayload(tflite::support::kTfLiteSupportPayload))
                                  .value_or(std::to_string(genericErrorCode)));
  } catch (std::invalid_argument &e) {
    // If non empty payload string cannot be converted to an integer. Set error code to 1(kError).
    errorCode = TFLSupportErrorCodeUnspecifiedError;
  }

  // If errorCode is outside the range of enum values possible or is
  // TFLSupportErrorCodeUnspecifiedError, we try to map the absl::Status::code() to assign
  // appropriate TFLSupportErrorCode or TFLSupportErrorCodeUnspecifiedError in default cases. Note:
  // The mapping to absl::Status::code() is done to generate a more specific error code than
  // TFLSupportErrorCodeUnspecifiedError in cases when the payload can't be mapped to
  // TfLiteSupportStatus. This can happen when absl::Status returned by TfLite are in turn returned
  // without moodification by TfLite Support Methods.
  if (errorCode > TFLErrorCodeLast || errorCode <= TFLErrorCodeFirst) {
    switch (status.code()) {
      case absl::StatusCode::kInternal:
        errorCode = TFLSupportErrorCodeInternalError;
        break;
      case absl::StatusCode::kInvalidArgument:
        errorCode = TFLSupportErrorCodeInvalidArgumentError;
        break;
      case absl::StatusCode::kNotFound:
        errorCode = TFLSupportErrorCodeNotFoundError;
        break;
      default:
        errorCode = TFLSupportErrorCodeUnspecifiedError;
        break;
    }
  }

  // Creates the NSEror with the appropriate error
  // TFLSupportErrorCode and message. TFLSupportErrorCode has a one to one
  // mapping with TfLiteSupportStatus starting from the value 1(TFLSupportErrorCodeUnspecifiedError)
  // and hence will be correctly initialized if directly cast from the integer code derived from
  // TfLiteSupportStatus stored in its payload. TFLSupportErrorCode omits kOk = 0 of
  // TfLiteSupportStatus.
  //
  // Stores a string including absl status code and message(if non empty) as the
  // error message See
  // https://github.com/abseil/abseil-cpp/blob/master/absl/status/status.h#L514
  // for explanation. absl::Status::message() can also be used but not always
  // guaranteed to be non empty.
  NSString *description = [NSString
      stringWithCString:status.ToString(absl::StatusToStringMode::kWithNoExtraData).c_str()
               encoding:NSUTF8StringEncoding];
  [TFLCommonUtils createCustomError:error withCode:errorCode description:description];
  return NO;
}

@end
