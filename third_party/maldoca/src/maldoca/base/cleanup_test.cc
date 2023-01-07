// Copyright 2021 Google LLC
// Copyright 2018 ZetaSQL Authors
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

#include "maldoca/base/cleanup.h"

#include <functional>
#include <type_traits>

#include "gtest/gtest.h"

namespace maldoca {

using AnyCleanup = Cleanup<absl::cleanup_internal::Tag, std::function<void()>>;

template <typename T1, typename T2>
void AssertTypeEq() {
  static_assert(std::is_same<T1, T2>::value, "unexpected type");
}

TEST(CleanupTest, BasicLambda) {
  std::string s = "active";
  {
    auto s_cleaner = MakeCleanup([&s] { s.assign("cleaned"); });
    EXPECT_EQ("active", s);
  }
  EXPECT_EQ("cleaned", s);
}

TEST(FinallyTest, NoCaptureLambda) {
  // Noncapturing lambdas are just structs and use aggregate initializers.
  // Make sure MakeCleanup is compatible with that kind of initialization.
  static std::string& s = *new std::string;
  s.assign("active");
  {
    auto s_cleaner = MakeCleanup([] { s.append(" clean"); });
    EXPECT_EQ("active", s);
  }
  EXPECT_EQ("active clean", s);
}

TEST(CleanupTest, Cancel) {
  std::string s = "active";
  {
    auto s_cleaner = MakeCleanup([&s] { s.assign("cleaned"); });
    EXPECT_EQ("active", s);
    std::move(s_cleaner).Cancel();
  }
  EXPECT_EQ("active", s);  // no cleanup should have occurred.
}

TEST(FinallyTest, TypeErasedWithoutFactory) {
  std::string s = "active";
  {
    AnyCleanup s_cleaner([&s] { s.append(" clean"); });
    EXPECT_EQ("active", s);
  }
  EXPECT_EQ("active clean", s);
}

struct Appender {
  Appender(std::string* s, const std::string& msg) : s_(s), msg_(msg) {}
  void operator()() const { s_->append(msg_); }
  std::string* s_;
  std::string msg_;
};

TEST(CleanupTest, NonLambda) {
  std::string s = "active";
  {
    auto c = MakeCleanup(Appender(&s, " cleaned"));
    AssertTypeEq<decltype(c), Cleanup<absl::cleanup_internal::Tag, Appender>>();
    EXPECT_EQ("active", s);
  }
  EXPECT_EQ("active cleaned", s);
}

class CleanupReferenceTest : public testing::Test {
 public:
  struct F {
    int* cp;
    int* i;
    F(int* cp, int* i) : cp(cp), i(i) {}
    F(const F& o) : cp(o.cp), i(o.i) { ++*cp; }
    F& operator=(const F& o) {
      cp = o.cp;
      i = o.i;
      ++*cp;
      return *this;
    }
    F(F&&) = default;
    F& operator=(F&&) = default;
    void operator()() const { ++*i; }
  };
  int copies_ = 0;
  int calls_ = 0;
  F f_ = F(&copies_, &calls_);

  static int g_calls;
  void SetUp() override { g_calls = 0; }
  static void CleanerFunction() { ++g_calls; }
};
int CleanupReferenceTest::g_calls = 0;

TEST_F(CleanupReferenceTest, FunctionPointer) {
  {
    auto c = MakeCleanup(&CleanerFunction);
    AssertTypeEq<decltype(c),
                 Cleanup<absl::cleanup_internal::Tag, void (*)()>>();
    EXPECT_EQ(0, g_calls);
  }
  EXPECT_EQ(1, g_calls);
  // Test that a function reference decays to a function pointer.
  {
    auto c = MakeCleanup(CleanerFunction);
    AssertTypeEq<decltype(c),
                 Cleanup<absl::cleanup_internal::Tag, void (*)()>>();
    EXPECT_EQ(1, g_calls);
  }
  EXPECT_EQ(2, g_calls);
}

TEST_F(CleanupReferenceTest, AssignLvalue) {
  std::string s = "0";
  Appender app1(&s, "1");
  Appender app2(&s, "2");
  {
    auto c = MakeCleanup(app1);
    std::move(c).Cancel();
    auto d = MakeCleanup(app2);
    EXPECT_EQ("0", s);
    app1();
    EXPECT_EQ("01", s);
  }
  EXPECT_EQ("012", s);
}

TEST_F(CleanupReferenceTest, FunctorRvalue) {
  {
    auto c = MakeCleanup(std::move(f_));
    AssertTypeEq<decltype(c), Cleanup<absl::cleanup_internal::Tag, F>>();
    EXPECT_EQ(0, copies_);
    EXPECT_EQ(0, calls_);
  }
  EXPECT_EQ(0, copies_);
  EXPECT_EQ(1, calls_);
}

TEST_F(CleanupReferenceTest, FunctorReferenceWrapper) {
  {
    auto c = MakeCleanup(std::cref(f_));
    AssertTypeEq<decltype(c), Cleanup<absl::cleanup_internal::Tag,
                                      std::reference_wrapper<const F>>>();
    EXPECT_EQ(0, copies_);
    EXPECT_EQ(0, calls_);
  }
  EXPECT_EQ(0, copies_);
  EXPECT_EQ(1, calls_);
}

}  // namespace maldoca
