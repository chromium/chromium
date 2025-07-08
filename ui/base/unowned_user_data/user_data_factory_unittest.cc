// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/unowned_user_data/user_data_factory.h"

#include <sstream>
#include <string>
#include <string_view>

#include "base/no_destructor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace ui {

namespace {

class TestFeatures;

// The first kind of user data that will be created by the "features" object.
class ScopedUserData1 {
 public:
  DECLARE_USER_DATA(ScopedUserData1);
  ScopedUserData1(UnownedUserDataHost& host, int value)
      : scoped_data_(host, *this), value_(value) {}
  virtual ~ScopedUserData1() = default;

  int value() const { return value_; }

  static ScopedUserData1* From(TestFeatures&);

 private:
  ScopedUnownedUserData<ScopedUserData1> scoped_data_;
  const int value_;
};

// The concrete implementation of the first user data type.
class ConcreteScopedUserData1 : public ScopedUserData1 {
 public:
  explicit ConcreteScopedUserData1(UnownedUserDataHost& host,
                                   int val1,
                                   int val2)
      : ScopedUserData1(host, val1 + val2) {}
  ~ConcreteScopedUserData1() override = default;
};

// The test-specific override implementation for the first user data type.
class TestScopedUserData1 : public ScopedUserData1 {
 public:
  static constexpr int kTestValue = 999;
  explicit TestScopedUserData1(UnownedUserDataHost& host)
      : ScopedUserData1(host, kTestValue) {}
  ~TestScopedUserData1() override = default;
};

// The second kind of user data that will be created by the "features" object.
class ScopedUserData2 {
 public:
  DECLARE_USER_DATA(ScopedUserData2);
  ScopedUserData2(UnownedUserDataHost& host, std::string_view value)
      : scoped_data_(host, *this), value_(value) {}
  virtual ~ScopedUserData2() = default;

  std::string_view value() const { return value_; }

  static ScopedUserData2* From(TestFeatures&);

 private:
  ScopedUnownedUserData<ScopedUserData2> scoped_data_;
  const std::string value_;
};

// The concrete implementation of the second user data type.
class ConcreteScopedUserData2 : public ScopedUserData2 {
 public:
  explicit ConcreteScopedUserData2(UnownedUserDataHost& host,
                                   std::string_view str1,
                                   std::string_view str2)
      : ScopedUserData2(host, std::string(str1) + std::string(str2)) {}
  ~ConcreteScopedUserData2() override = default;
};

// The test-specific override implementation for the second user data type.
class TestScopedUserData2 : public ScopedUserData2 {
 public:
  explicit TestScopedUserData2(UnownedUserDataHost& host,
                               std::string_view value)
      : ScopedUserData2(host, value) {}
  ~TestScopedUserData2() override = default;
};

// The test "features" object which creates the user data and adds it to an
// unowned user data host. Equivalent to TabFeatures or BrowserWindowFeatures.
class TestFeatures {
 public:
  static constexpr int kIntVal1 = 3;
  static constexpr int kIntVal2 = 10;
  static constexpr int kSum = kIntVal1 + kIntVal2;
  static constexpr std::string kStringVal1 = "foo";
  static constexpr std::string kStringVal2 = "bar";
  static constexpr std::string kConcat = kStringVal1 + kStringVal2;

  TestFeatures() {
    data1_ = GetDataFactory().CreateInstance<ConcreteScopedUserData1>(
        *this, unowned_data_host(), kIntVal1, kIntVal2);
    data2_ = GetDataFactory().CreateInstance<ConcreteScopedUserData2>(
        *this, unowned_data_host(), kStringVal1, kStringVal2);
  }

  ~TestFeatures() = default;

  UnownedUserDataHost& unowned_data_host() { return host_; }

  // The factory accessor. Note that in this case, there is no "model" object
  // so the "features" object is the owner. If there was a separate model object
  // which owned this features object, it might make sense to make that the
  // owner type for the factory instead.
  static UserDataFactoryWithOwner<TestFeatures>& GetDataFactory() {
    static base::NoDestructor<UserDataFactoryWithOwner<TestFeatures>> instance;
    return *instance;
  }

 private:
  UnownedUserDataHost host_;
  std::unique_ptr<ScopedUserData1> data1_;
  std::unique_ptr<ScopedUserData2> data2_;
};

DEFINE_USER_DATA(ScopedUserData1);

ScopedUserData1* ScopedUserData1::From(TestFeatures& features) {
  return Get(features.unowned_data_host());
}

DEFINE_USER_DATA(ScopedUserData2);

ScopedUserData2* ScopedUserData2::From(TestFeatures& features) {
  return Get(features.unowned_data_host());
}

}  // namespace

TEST(UserDataFactoryTest, CreatesDefaults) {
  TestFeatures features;
  auto* const data1 = ScopedUserData1::From(features);
  auto* const data2 = ScopedUserData2::From(features);
  ASSERT_NE(data1, nullptr);
  ASSERT_NE(data2, nullptr);
  ASSERT_NE(static_cast<void*>(data1), static_cast<void*>(data2));
  EXPECT_EQ(TestFeatures::kSum, data1->value());
  EXPECT_EQ(TestFeatures::kConcat, data2->value());
}

TEST(UserDataFactoryTest, OverrideFirst) {
  auto factory_override = TestFeatures::GetDataFactory().AddOverrideForTesting(
      base::BindRepeating([](TestFeatures& host) {
        return std::make_unique<TestScopedUserData1>(host.unowned_data_host());
      }));

  // Ensure that only the first data is overridden.
  TestFeatures features;
  auto* const data1 = ScopedUserData1::From(features);
  auto* const data2 = ScopedUserData2::From(features);
  EXPECT_EQ(TestScopedUserData1::kTestValue, data1->value());
  EXPECT_EQ(TestFeatures::kConcat, data2->value());

  // Ensure this extends to other features objects that are created.
  TestFeatures features2;
  EXPECT_EQ(TestScopedUserData1::kTestValue,
            ScopedUserData1::Get(features2.unowned_data_host())->value());
}

TEST(UserDataFactoryTest, ScopedOverrideGoesOutOfScope) {
  {
    auto factory_override =
        TestFeatures::GetDataFactory().AddOverrideForTesting(
            base::BindRepeating([](TestFeatures& host) {
              return std::make_unique<TestScopedUserData1>(
                  host.unowned_data_host());
            }));

    // Ensure that only the first data is overridden.
    TestFeatures features;
    auto* const data1 = ScopedUserData1::From(features);
    auto* const data2 = ScopedUserData2::From(features);
    EXPECT_EQ(TestScopedUserData1::kTestValue, data1->value());
    EXPECT_EQ(TestFeatures::kConcat, data2->value());
  }

  // After the override goes out of scope, the default behavior is restored for
  // new features objects.
  TestFeatures features2;
  EXPECT_EQ(TestFeatures::kSum,
            ScopedUserData1::Get(features2.unowned_data_host())->value());
}

TEST(UserDataFactoryTest, ScopedOverrideCopyAndReset) {
  auto factory_override = TestFeatures::GetDataFactory().AddOverrideForTesting(
      base::BindRepeating([](TestFeatures& host) {
        return std::make_unique<TestScopedUserData1>(host.unowned_data_host());
      }));

  // Ensure that only the first data is overridden.
  TestFeatures features;
  auto* const data1 = ScopedUserData1::From(features);
  auto* const data2 = ScopedUserData2::From(features);
  EXPECT_EQ(TestScopedUserData1::kTestValue, data1->value());
  EXPECT_EQ(TestFeatures::kConcat, data2->value());

  factory_override = UserDataFactory::ScopedOverride();

  // After the override goes out of scope, the default behavior is restored for
  // new features objects.
  TestFeatures features2;
  EXPECT_EQ(TestFeatures::kSum,
            ScopedUserData1::Get(features2.unowned_data_host())->value());
}

TEST(UserDataFactoryTest, OverrideSecond) {
  auto factory_override = TestFeatures::GetDataFactory().AddOverrideForTesting(
      base::BindRepeating([](TestFeatures& host) {
        // Use the value from another object already in `host` to calculate
        // the value.
        std::ostringstream oss;
        oss << ScopedUserData1::Get(host.unowned_data_host())->value();
        return std::make_unique<TestScopedUserData2>(host.unowned_data_host(),
                                                     oss.str());
      }));

  // This is the string representation of the value that will be read out of
  // the first data and written into the second as a string.
  constexpr char kExpected[] = "13";

  // Ensure that only the first data is overridden.
  TestFeatures features;
  auto* const data1 = ScopedUserData1::From(features);
  auto* const data2 = ScopedUserData2::From(features);
  EXPECT_EQ(TestFeatures::kSum, data1->value());
  EXPECT_EQ(kExpected, data2->value());

  // Ensure this extends to other features objects that are created.
  TestFeatures features2;
  EXPECT_EQ(kExpected,
            ScopedUserData2::Get(features2.unowned_data_host())->value());
}

TEST(UserDataFactoryTest, OverrideBoth) {
  auto factory_override = TestFeatures::GetDataFactory().AddOverrideForTesting(
      base::BindRepeating([](TestFeatures& host) {
        return std::make_unique<TestScopedUserData1>(host.unowned_data_host());
      }));
  auto factory_override2 = TestFeatures::GetDataFactory().AddOverrideForTesting(
      base::BindRepeating([](TestFeatures& host) {
        // Use the value from another object already in `host` to calculate
        // the value.
        std::ostringstream oss;
        oss << ScopedUserData1::Get(host.unowned_data_host())->value();
        return std::make_unique<TestScopedUserData2>(host.unowned_data_host(),
                                                     oss.str());
      }));

  // This is the string representation of the value that will be read out of
  // the first data and written into the second as a string.
  constexpr char kExpected[] = "999";

  // Ensure that only the first data is overridden.
  TestFeatures features;
  auto* const data1 = ScopedUserData1::From(features);
  auto* const data2 = ScopedUserData2::From(features);
  EXPECT_EQ(TestScopedUserData1::kTestValue, data1->value());
  EXPECT_EQ(kExpected, data2->value());
}

}  // namespace ui
