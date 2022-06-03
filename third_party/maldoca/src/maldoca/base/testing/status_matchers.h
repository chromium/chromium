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

#ifndef MALDOCA_BASE_TESTING_STATUS_MATCHERS_H_
#define MALDOCA_BASE_TESTING_STATUS_MATCHERS_H_

#ifdef MALDOCA_CHROME

#include <array>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>

#include "absl/base/casts.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/any.h"
#include "absl/utility/utility.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "maldoca/base/status_macros.h"

namespace maldoca {
namespace testing {

// Macros for testing the results of functions that return absl::Status or
// absl::StatusOr<T> (for any type T).
#define MALDOCA_EXPECT_OK(expression) \
  EXPECT_THAT(expression, ::maldoca::testing::IsOk())
#define MALDOCA_ASSERT_OK(expression) \
  ASSERT_THAT(expression, ::maldoca::testing::IsOk())
#define MALDOCA_ASSERT_OK_AND_ASSIGN(lhs, rexpr) \
  MALDOCA_ASSERT_OK_AND_ASSIGN_IMPL_(            \
      MALDOCA_MACROS_CONCAT_NAME_(_status_or, __LINE__), lhs, rexpr)
#define MALDOCA_ASSERT_OK_AND_ASSIGN_IMPL_(statusor, lhs, rexpr) \
  auto statusor = (rexpr);                                       \
  MALDOCA_ASSERT_OK(statusor);                                   \
  lhs = std::move(statusor).value();

// Per https://github.com/abseil/abseil-cpp/issues/951 copy the test code

#ifdef GTEST_HAS_STATUS_MATCHERS
using ::testing::status::IsOk;
using ::testing::status::IsOkAndHolds;
#else  // GTEST_HAS_STATUS_MATCHERS
namespace internal {

// This function and its overload allow the same matcher to be used for Status
// and StatusOr tests.
inline absl::Status GetStatus(const absl::Status& status) { return status; }

template <typename T>
inline absl::Status GetStatus(const absl::StatusOr<T>& statusor) {
  return statusor.status();
}

// Monomorphic implementation of matcher IsOkAndHolds(m).  StatusOrType is a
// reference to StatusOr<T>.
template <typename StatusOrType>
class IsOkAndHoldsMatcherImpl
    : public ::testing::MatcherInterface<StatusOrType> {
 public:
  typedef
      typename std::remove_reference<StatusOrType>::type::value_type value_type;

  template <typename InnerMatcher>
  explicit IsOkAndHoldsMatcherImpl(InnerMatcher&& inner_matcher)
      : inner_matcher_(::testing::SafeMatcherCast<const value_type&>(
            std::forward<InnerMatcher>(inner_matcher))) {}

  void DescribeTo(std::ostream* os) const override {
    *os << "is OK and has a value that ";
    inner_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "isn't OK or has a value that ";
    inner_matcher_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(
      StatusOrType actual_value,
      ::testing::MatchResultListener* result_listener) const override {
    if (!actual_value.ok()) {
      *result_listener << "which has status " << actual_value.status();
      return false;
    }

    ::testing::StringMatchResultListener inner_listener;
    const bool matches =
        inner_matcher_.MatchAndExplain(*actual_value, &inner_listener);
    const std::string inner_explanation = inner_listener.str();
    if (!inner_explanation.empty()) {
      *result_listener << "which contains value "
                       << ::testing::PrintToString(*actual_value) << ", "
                       << inner_explanation;
    }
    return matches;
  }

 private:
  const ::testing::Matcher<const value_type&> inner_matcher_;
};

// Implements IsOkAndHolds(m) as a polymorphic matcher.
template <typename InnerMatcher>
class IsOkAndHoldsMatcher {
 public:
  explicit IsOkAndHoldsMatcher(InnerMatcher inner_matcher)
      : inner_matcher_(std::move(inner_matcher)) {}

  // Converts this polymorphic matcher to a monomorphic matcher of the
  // given type.  StatusOrType can be either StatusOr<T> or a
  // reference to StatusOr<T>.
  template <typename StatusOrType>
  operator ::testing::Matcher<StatusOrType>() const {  // NOLINT
    return ::testing::Matcher<StatusOrType>(
        new IsOkAndHoldsMatcherImpl<const StatusOrType&>(inner_matcher_));
  }

 private:
  const InnerMatcher inner_matcher_;
};

// Monomorphic implementation of matcher IsOk() for a given type T.
// T can be Status, StatusOr<>, or a reference to either of them.
template <typename T>
class MonoIsOkMatcherImpl : public ::testing::MatcherInterface<T> {
 public:
  void DescribeTo(std::ostream* os) const override { *os << "is OK"; }
  void DescribeNegationTo(std::ostream* os) const override {
    *os << "is not OK";
  }
  bool MatchAndExplain(T actual_value,
                       ::testing::MatchResultListener*) const override {
    return GetStatus(actual_value).ok();
  }
};

// Implements IsOk() as a polymorphic matcher.
class IsOkMatcher {
 public:
  template <typename T>
  operator ::testing::Matcher<T>() const {  // NOLINT
    return ::testing::Matcher<T>(new MonoIsOkMatcherImpl<T>());
  }
};

template <typename StatusType>
class StatusIsImpl : public ::testing::MatcherInterface<StatusType> {
 public:
  StatusIsImpl(const ::testing::Matcher<absl::StatusCode>& code,
               const ::testing::Matcher<const std::string&>& message)
      : code_(code), message_(message) {}

  inline bool MatchAndExplain(StatusType status,
                              ::testing::MatchResultListener* listener) const {
    ::testing::StringMatchResultListener str_listener;
    absl::Status real_status = GetStatus(status);
    if (!code_.MatchAndExplain(real_status.code(), &str_listener)) {
      *listener << str_listener.str();
      return false;
    }
    if (!message_.MatchAndExplain(
            static_cast<std::string>(real_status.message()), &str_listener)) {
      *listener << str_listener.str();
      return false;
    }
    return true;
  }

  inline void DescribeTo(std::ostream* os) const {
    *os << "has a status code that ";
    code_.DescribeTo(os);
    *os << " and a message that ";
    message_.DescribeTo(os);
  }

  inline void DescribeNegationto(std::ostream* os) const {
    *os << "has a status code that ";
    code_.DescribeNegationTo(os);
    *os << " and a message that ";
    message_.DescribeNegationTo(os);
  }

 private:
  ::testing::Matcher<absl::StatusCode> code_;
  ::testing::Matcher<const std::string&> message_;
};

class StatusIsPoly {
 public:
  StatusIsPoly(::testing::Matcher<absl::StatusCode>&& code,
               ::testing::Matcher<const std::string&>&& message)
      : code_(code), message_(message) {}

  // Converts this polymorphic matcher to a monomorphic matcher.
  template <typename StatusType>
  operator ::testing::Matcher<StatusType>() const {
    return ::testing::Matcher<StatusType>(
        new StatusIsImpl<StatusType>(code_, message_));
  }

 private:
  ::testing::Matcher<absl::StatusCode> code_;
  ::testing::Matcher<const std::string&> message_;
};

}  // namespace internal

// Implements StatusIs() as a polymorphic matcher.

// Returns a gMock matcher that matches a StatusOr<> whose status is
// OK and whose value matches the inner matcher.
template <typename InnerMatcher>
internal::IsOkAndHoldsMatcher<typename std::decay<InnerMatcher>::type>
IsOkAndHolds(InnerMatcher&& inner_matcher) {
  return internal::IsOkAndHoldsMatcher<typename std::decay<InnerMatcher>::type>(
      std::forward<InnerMatcher>(inner_matcher));
}

// Returns a gMock matcher that matches a Status or StatusOr<> which is OK.
inline internal::IsOkMatcher IsOk() { return internal::IsOkMatcher(); }

// This function allows us to avoid a template parameter when writing tests, so
// that we can transparently test both Status and StatusOr returns.
inline internal::StatusIsPoly StatusIs(
    ::testing::Matcher<absl::StatusCode>&& code,
    ::testing::Matcher<const std::string&>&& message) {
  return internal::StatusIsPoly(
      std::forward< ::testing::Matcher<absl::StatusCode> >(code),
      std::forward< ::testing::Matcher<const std::string&> >(message));
}

inline internal::StatusIsPoly StatusIs(
    ::testing::Matcher<absl::StatusCode>&& code) {
  return internal::StatusIsPoly(
      std::forward< ::testing::Matcher<absl::StatusCode> >(code),
      std::forward< ::testing::Matcher<const std::string&> >(::testing::_));
}

#endif  // GTEST_HAS_STATUS_MATCHERS

}  // namespace testing
}  // namespace maldoca

#else  // MALDOCA_CHROME
#include "zetasql/base/testing/status_matchers.h"

namespace maldoca {
namespace testing {
#define MALDOCA_EXPECT_OK ZETASQL_EXPECT_OK
#define MALDOCA_ASSERT_OK ZETASQL_ASSERT_OK
#define MALDOCA_ASSERT_OK_AND_ASSIGN ZETASQL_ASSERT_OK_AND_ASSIGN
using ::zetasql_base::testing::IsOk;
using ::zetasql_base::testing::IsOkAndHolds;
using ::zetasql_base::testing::StatusIs;
}  // namespace testing
}  // namespace maldoca

#endif  // MALDOCA_CHROME
#endif  // MALDOCA_BASE_TESTING_STATUS_MATCHERS_H_
