/*
 * Copyright 2017 Google LLC
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

#ifndef FCP_TESTING_TESTING_H_
#define FCP_TESTING_TESTING_H_

#include <iostream>
#include <memory>
#include <string>
#include <type_traits>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "fcp/base/monitoring.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// This file defines platform dependent utilities for testing,
// based on the public version of googletest.

namespace fcp {

// Convenience macros for `EXPECT_THAT(s, IsOk())`, where `s` is either
// a `Status` or a `StatusOr<T>`.
// Old versions of the protobuf library define EXPECT_OK as well, so we only
// conditionally define our version.
#if !defined(EXPECT_OK)
#define EXPECT_OK(result) EXPECT_THAT(result, fcp::IsOk())
#endif
#define ASSERT_OK(result) ASSERT_THAT(result, fcp::IsOk())

/**
 * Polymorphic matchers for Status or StatusOr on status code.
 */
template <typename T>
bool IsCode(StatusOr<T> const& x, StatusCode code) {
  return x.status().code() == code;
}
inline bool IsCode(Status const& x, StatusCode code) {
  return x.code() == code;
}

template <typename T>
class StatusMatcherImpl : public ::testing::MatcherInterface<T> {
 public:
  explicit StatusMatcherImpl(StatusCode code) : code_(code) {}
  void DescribeTo(::std::ostream* os) const override {
    *os << "is " << absl::StatusCodeToString(code_);
  }
  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "is not " << absl::StatusCodeToString(code_);
  }
  bool MatchAndExplain(
      T x, ::testing::MatchResultListener* listener) const override {
    return IsCode(x, code_);
  }

 private:
  StatusCode code_;
};

class StatusMatcher {
 public:
  explicit StatusMatcher(StatusCode code) : code_(code) {}

  template <typename T>
  operator testing::Matcher<T>() const {  // NOLINT
    return ::testing::MakeMatcher(new StatusMatcherImpl<T>(code_));
  }

 private:
  StatusCode code_;
};

StatusMatcher IsCode(StatusCode code);
StatusMatcher IsOk();

}  // namespace fcp

#endif  // FCP_TESTING_TESTING_H_
