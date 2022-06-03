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

#ifndef MALDOCA_BASE_STATUS_MACROS_H_
#define MALDOCA_BASE_STATUS_MACROS_H_

#ifdef MALDOCA_CHROME
#include <sstream>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

// Implmentation
#define MALDOCA_MACROS_CONCAT_NAME_INNER_(x, y) x##y
#define MALDOCA_MACROS_CONCAT_NAME_(x, y) \
  MALDOCA_MACROS_CONCAT_NAME_INNER_(x, y)

#define MALDOCA_MACROS_IMPL_GET_VARIADIC2_HELPER_(_1, _2, NAME, ...) NAME
#define MALDOCA_MACROS_IMPL_GET_VARIADIC2_(args) \
  MALDOCA_MACROS_IMPL_GET_VARIADIC2_HELPER_ args

#define MALDOCA_MACROS_IMPL_GET_VARIADIC3_HELPER_(_1, _2, _3, NAME, ...) NAME
#define MALDOCA_MACROS_IMPL_GET_VARIADIC3_(args) \
  MALDOCA_MACROS_IMPL_GET_VARIADIC3_HELPER_ args

// WARNING, this expands into multipel lines so can't be bare, E.g., no
// if (..) MALDOCA_ASSIGN_OR_RETURN(...);  but only if(...) {...}
#define MALDOCA_ASSIGN_OR_RETURN_IMPL_(statusor, lhs, rexpr, error_exp)      \
  auto statusor = (rexpr);                                                   \
  if (ABSL_PREDICT_FALSE(!statusor.ok())) {                                  \
    absl::Status old_status(std::move(statusor).status());                   \
    std::stringstream _;                                                     \
    error_exp;                                                               \
    std::string error_msg = _.str();                                         \
    if (error_msg.empty()) {                                                 \
      return old_status;                                                     \
    }                                                                        \
    return absl::Status(old_status.code(),                                   \
                        absl::StrCat(old_status.message(), " ", error_msg)); \
  }                                                                          \
  lhs = std::move(statusor).value();

#define MALDOCA_ASSIGN_OR_RETURN_3_(lhs, rexpr, error_exp)           \
  MALDOCA_ASSIGN_OR_RETURN_IMPL_(                                    \
      MALDOCA_MACROS_CONCAT_NAME_(_status_or, __LINE__), lhs, rexpr, \
      error_exp)

#define MALDOCA_ASSIGN_OR_RETURN_2_(lhs, rexpr) \
  MALDOCA_ASSIGN_OR_RETURN_3_(lhs, rexpr, _ << "")

#define MALDOCA_RETURN_IF_ERROR_2_(expr, error_exp)                            \
  do {                                                                         \
    auto old_status = (expr);                                                  \
    if (ABSL_PREDICT_FALSE(!old_status.ok())) {                                \
      std::stringstream _;                                                     \
      error_exp;                                                               \
      std::string error_msg = _.str();                                         \
      if (error_msg.empty()) {                                                 \
        return old_status;                                                     \
      }                                                                        \
      return absl::Status(old_status.code(),                                   \
                          absl::StrCat(old_status.message(), " ", error_msg)); \
    }                                                                          \
  } while (false)

#define MALDOCA_RETURN_IF_ERROR_1_(expr) \
  MALDOCA_RETURN_IF_ERROR_2_(expr, _ << "")

// "public" macros
#define MALDOCA_ASSIGN_OR_RETURN(...)                                          \
  MALDOCA_MACROS_IMPL_GET_VARIADIC3_(                                          \
      (__VA_ARGS__, MALDOCA_ASSIGN_OR_RETURN_3_, MALDOCA_ASSIGN_OR_RETURN_2_)) \
  (__VA_ARGS__)

#define MALDOCA_RETURN_IF_ERROR(...)                                         \
  MALDOCA_MACROS_IMPL_GET_VARIADIC2_(                                        \
      (__VA_ARGS__, MALDOCA_RETURN_IF_ERROR_2_, MALDOCA_RETURN_IF_ERROR_1_)) \
  (__VA_ARGS__)

#else  // MALDOCA_CHROME
#include "zetasql/base/status_macros.h"

#define MALDOCA_RETURN_IF_ERROR ZETASQL_RETURN_IF_ERROR
#define MALDOCA_ASSIGN_OR_RETURN ZETASQL_ASSIGN_OR_RETURN

#endif  // MALDOCA_CHROME

#endif  // MALDOCA_BASE_STATUS_MACROS_H_
