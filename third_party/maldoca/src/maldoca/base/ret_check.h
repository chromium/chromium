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

#ifndef MALDOCA_BASE_RET_CHECK_H_
#define MALDOCA_BASE_RET_CHECK_H_

#ifdef MALDOCA_CHROME
#include <sstream>

#include "maldoca/base/status_macros.h"

namespace {
inline const absl::Status& AsStatus(const absl::Status& status) {
  return status;
}

template <typename T>
inline const absl::Status& AsStatus(const absl::StatusOr<T>& status_or) {
  return status_or.status();
}
}  // namespace

#define MALDOCA_RET_CHECK_2_(expr, error_exp) \
  do {                                        \
    if (ABSL_PREDICT_FALSE(!(expr))) {        \
      std::stringstream _;                    \
      error_exp;                              \
      return absl::InternalError(_.str());    \
    }                                         \
  } while (false)
#define MALDOCA_RET_CHECK_1_(expr) MALDOCA_RET_CHECK_2_(expr, _)

#define MALDOCA_RET_CHECK(...)                                   \
  MALDOCA_MACROS_IMPL_GET_VARIADIC2_(                            \
      (__VA_ARGS__, MALDOCA_RET_CHECK_2_, MALDOCA_RET_CHECK_1_)) \
  (__VA_ARGS__)

#define MALDOCA_RET_CHECK_FAIL          \
  do {                                  \
    return absl::InternalError("FAIL"); \
  } while (false)

#define MALDOCA_RET_CHECK_OK(expr, ...) \
  MALDOCA_RETURN_IF_ERROR(AsStatus(expr, __VA_ARGS__));

#define MALDOCA_RET_CHECK_EQ(lhs, rhs, ...) \
  MALDOCA_RET_CHECK_OP_IMPL_(==, lhs, rhs, __VA_ARGS__)
#define MALDOCA_RET_CHECK_NE(lhs, rhs, ...) \
  MALDOCA_RET_CHECK_OP_IMPL_(!=, lhs, rhs, __VA_ARGS__)
#define MALDOCA_RET_CHECK_LE(lhs, rhs, ...) \
  MALDOCA_RET_CHECK_OP_IMPL_(<=, lhs, rhs, __VA_ARGS__)
#define MALDOCA_RET_CHECK_LT(lhs, rhs, ...) \
  MALDOCA_RET_CHECK_OP_IMPL_(<, lhs, rhs, __VA_ARGS__)
#define MALDOCA_RET_CHECK_GE(lhs, rhs, ...) \
  MALDOCA_RET_CHECK_OP_IMPL_(>=, lhs, rhs, __VA_ARGS__)
#define MALDOCA_RET_CHECK_GT(lhs, rhs, ...) \
  MALDOCA_RET_CHECK_OP_IMPL_(>, lhs, rhs, __VA_ARGS__)

#define MALDOCA_RET_CHECK_OP_IMPL_(op, lhs, rhs, ...) \
  MALDOCA_RET_CHECK(((lhs)op(rhs)), __VA_ARGS__)

#else

#include "zetasql/base/ret_check.h"

#define MALDOCA_RET_CHECK ZETASQL_RET_CHECK
#define MALDOCA_RET_CHECK_FAIL ZETASQL_RET_CHECK_FAIL
#define MALDOCA_RET_CHECK_OK ZETASQL_RET_CHECK_OK
#define MALDOCA_RET_CHECK_EQ ZETASQL_RET_CHECK_EQ
#define MALDOCA_RET_CHECK_NE ZETASQL_RET_CHECK_NE
#define MALDOCA_RET_CHECK_LE ZETASQL_RET_CHECK_LE
#define MALDOCA_RET_CHECK_LT ZETASQL_RET_CHECK_LT
#define MALDOCA_RET_CHECK_GE ZETASQL_RET_CHECK_GE
#define MALDOCA_RET_CHECK_GT ZETASQL_RET_CHECK_GT
#endif  // MALDOCA_CHROME
#endif  // MALDOCA_BASE_RET_CHECK_H_
