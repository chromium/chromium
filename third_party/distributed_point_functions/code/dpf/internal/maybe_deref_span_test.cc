// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "dpf/internal/maybe_deref_span.h"

#include <type_traits>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace distributed_point_functions {
namespace dpf_internal {
namespace {

using T = int;

TEST(MaybeDerefSpanTest, TestExplicitMutableDirectSpan) {
  std::vector<T> x = {1, 2};
  absl::Span<T> span(x);
  MaybeDerefSpan<T> span2(span);

  EXPECT_EQ(span2.size(), x.size());
  EXPECT_EQ(span2[0], x[0]);
  EXPECT_EQ(span2[1], x[1]);
  EXPECT_EQ(&span2[0], &x[0]);
  EXPECT_EQ(&span2[1], &x[1]);

  span2[0] = 3;
  EXPECT_EQ(span2[0], x[0]);
  EXPECT_EQ(x[0], 3);
}

TEST(MaybeDerefSpanTest, TestExplicitMutableSpan) {
  const std::vector<T> x = {1, 2};
  absl::Span<const T> span(x);
  MaybeDerefSpan<const T> span2(span);

  EXPECT_EQ(span2.size(), x.size());
  EXPECT_EQ(span2[0], x[0]);
  EXPECT_EQ(span2[1], x[1]);
  EXPECT_EQ(&span2[0], &x[0]);
  EXPECT_EQ(&span2[1], &x[1]);
}

TEST(MaybeDerefSpanTest, TestExplicitMutablePointerSpan) {
  std::vector<T> x = {1, 2};
  std::vector<T*> x2 = {&x[0], &x[1]};
  absl::Span<T*> span(x2);
  MaybeDerefSpan<T> span2(span);

  EXPECT_EQ(span2.size(), x.size());
  EXPECT_EQ(span2[0], x[0]);
  EXPECT_EQ(span2[1], x[1]);
  EXPECT_EQ(&span2[0], &x[0]);
  EXPECT_EQ(&span2[1], &x[1]);

  span2[0] = 3;
  EXPECT_EQ(span2[0], x[0]);
  EXPECT_EQ(x[0], 3);
}

TEST(MaybeDerefSpanTest, TestExplicitMutablePointerConstSpan) {
  std::vector<T> x = {1, 2};
  const std::vector<T*> x2 = {&x[0], &x[1]};
  absl::Span<T* const> span(x2);
  MaybeDerefSpan<T> span2(span);

  EXPECT_EQ(span2.size(), x.size());
  EXPECT_EQ(span2[0], x[0]);
  EXPECT_EQ(span2[1], x[1]);
  EXPECT_EQ(&span2[0], &x[0]);
  EXPECT_EQ(&span2[1], &x[1]);
}

TEST(MaybeDerefSpanTest, TestExplicitConstPointerConstSpan) {
  const std::vector<T> x = {1, 2};
  const std::vector<const T*> x2 = {&x[0], &x[1]};
  absl::Span<const T* const> span(x2);
  MaybeDerefSpan<const T> span2(span);

  EXPECT_EQ(span2.size(), x.size());
  EXPECT_EQ(span2[0], x[0]);
  EXPECT_EQ(span2[1], x[1]);
  EXPECT_EQ(&span2[0], &x[0]);
  EXPECT_EQ(&span2[1], &x[1]);
}

TEST(MaybeDerefSpanTest, TestMutableSpanToConstSpan) {
  std::vector<T> x = {1, 2};
  absl::Span<T> span(x);
  MaybeDerefSpan<T> span2(span);
  MaybeDerefSpan<const T> span3(span2);

  EXPECT_EQ(span3.size(), x.size());
  EXPECT_EQ(span3[0], x[0]);
  EXPECT_EQ(span3[1], x[1]);
  EXPECT_EQ(&span3[0], &x[0]);
  EXPECT_EQ(&span3[1], &x[1]);
}

TEST(MaybeDerefSpanTest, TestImplicitConstSpan) {
  const std::vector<T> x = {1, 2};
  MaybeDerefSpan<const T> span2(x);

  EXPECT_EQ(span2.size(), x.size());
  EXPECT_EQ(span2[0], x[0]);
  EXPECT_EQ(span2[1], x[1]);
  EXPECT_EQ(&span2[0], &x[0]);
  EXPECT_EQ(&span2[1], &x[1]);
}

TEST(MaybeDerefSpanTest, TestImplicitPointerConstSpan) {
  const std::vector<T> x = {1, 2};
  const std::vector<const T*> x2 = {&x[0], &x[1]};
  MaybeDerefSpan<const T> span2(x2);

  EXPECT_EQ(span2.size(), x.size());
  EXPECT_EQ(span2[0], x[0]);
  EXPECT_EQ(span2[1], x[1]);
  EXPECT_EQ(&span2[0], &x[0]);
  EXPECT_EQ(&span2[1], &x[1]);
}

void TestEq(MaybeDerefSpan<const T> span, const std::vector<T>& vector) {
  EXPECT_EQ(span.size(), vector.size());
  EXPECT_EQ(span[0], vector[0]);
  EXPECT_EQ(span[1], vector[1]);
  EXPECT_EQ(&span[0], &vector[0]);
  EXPECT_EQ(&span[1], &vector[1]);
}

TEST(MaybeDerefSpanTest, TestFunctionCallMutableVector) {
  std::vector<T> x = {1, 2};

  TestEq(x, x);
}

TEST(MaybeDerefSpanTest, TestFunctionCallMutablePointerVector) {
  std::vector<T> x = {1, 2};
  std::vector<T*> x2 = {&x[0], &x[1]};

  TestEq(x2, x);
}

TEST(MaybeDerefSpanTest, TestFunctionCallConstVector) {
  const std::vector<T> x = {1, 2};

  TestEq(x, x);
}

TEST(MaybeDerefSpanTest, TestFunctionCallMutablePointerConstVector) {
  std::vector<T> x = {1, 2};
  const std::vector<T*> x2 = {&x[0], &x[1]};

  TestEq(x2, x);
}

TEST(MaybeDerefSpanTest, TestFunctionCallConstPointerConstVector) {
  const std::vector<T> x = {1, 2};
  const std::vector<const T*> x2 = {&x[0], &x[1]};

  TestEq(x2, x);
}

// Taken from https://en.cppreference.com/w/cpp/types/is_convertible.
template <class From, class To>
auto test_implicitly_convertible(int)
    -> decltype(void(std::declval<void (&)(To)>()(std::declval<From>())),
                std::true_type{});
template <class, class>
auto test_implicitly_convertible(...) -> std::false_type;

// Test that vectors are convertible only to const spans.
static_assert(
    decltype(test_implicitly_convertible<std::vector<T>, MaybeDerefSpan<T>>(
        0))::value == false);
static_assert(decltype(test_implicitly_convertible<
                       std::vector<T>, MaybeDerefSpan<const T>>(0))::value ==
              true);

}  // namespace
}  // namespace dpf_internal
}  // namespace distributed_point_functions
