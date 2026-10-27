// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace ui {

namespace {

// One user data class.
class TestScopedUnownedUserData {
 public:
  DECLARE_USER_DATA(TestScopedUnownedUserData);

  explicit TestScopedUnownedUserData(UnownedUserDataHost& host)
      : scoped_data_holder_(host, *this) {}
  ~TestScopedUnownedUserData() = default;

 private:
  ScopedUnownedUserData<TestScopedUnownedUserData> scoped_data_holder_;
};

DEFINE_USER_DATA(TestScopedUnownedUserData);

// Another, different user data class.
class TestScopedUnownedUserData2 {
 public:
  DECLARE_USER_DATA(TestScopedUnownedUserData2);

  explicit TestScopedUnownedUserData2(UnownedUserDataHost& host)
      : scoped_data_(host, *this) {}
  ~TestScopedUnownedUserData2() = default;

 private:
  ScopedUnownedUserData<TestScopedUnownedUserData2> scoped_data_;
};

DEFINE_USER_DATA(TestScopedUnownedUserData2);

class ClassWithoutUserData {};

class DerivedWithoutDuplicateUserData : public TestScopedUnownedUserData {
 public:
  using TestScopedUnownedUserData::TestScopedUnownedUserData;
};

class GrandchildWithoutDuplicateUserData
    : public DerivedWithoutDuplicateUserData {
 public:
  using DerivedWithoutDuplicateUserData::DerivedWithoutDuplicateUserData;
};

class MultiDerivedWithDuplicateUserData : public TestScopedUnownedUserData,
                                          public TestScopedUnownedUserData2 {};

}  // namespace

// Declared outside the anonymous namespace so `DECLARE_USER_DATA`'s
// `kDataKeyImplProvider` does not trigger `-Wundefined-internal` without
// `DEFINE_USER_DATA`.
class DerivedWithDuplicateUserData : public TestScopedUnownedUserData {
 public:
  DECLARE_USER_DATA(DerivedWithDuplicateUserData);
};

class GrandchildWithDuplicateUserData : public DerivedWithoutDuplicateUserData {
 public:
  DECLARE_USER_DATA(GrandchildWithDuplicateUserData);
};

static_assert(
    !internal::SingleUnownedUserDataInHierarchy<ClassWithoutUserData>);
static_assert(
    internal::SingleUnownedUserDataInHierarchy<TestScopedUnownedUserData>);
static_assert(internal::SingleUnownedUserDataInHierarchy<
              DerivedWithoutDuplicateUserData>);
static_assert(
    !internal::SingleUnownedUserDataInHierarchy<DerivedWithDuplicateUserData>);
static_assert(internal::SingleUnownedUserDataInHierarchy<
              GrandchildWithoutDuplicateUserData>);
static_assert(!internal::SingleUnownedUserDataInHierarchy<
              GrandchildWithDuplicateUserData>);
static_assert(!internal::SingleUnownedUserDataInHierarchy<
              MultiDerivedWithDuplicateUserData>);

// Tests basic functionality of a class with a ScopedUnownedUserData member.
TEST(UnownedUserDataTest, ScopedUnownedUserData) {
  UnownedUserDataHost host;
  EXPECT_EQ(nullptr, TestScopedUnownedUserData::Get(host));
  {
    TestScopedUnownedUserData test_data(host);
    EXPECT_EQ(&test_data, TestScopedUnownedUserData::Get(host));
  }
  EXPECT_EQ(nullptr, TestScopedUnownedUserData::Get(host));
}

TEST(UnownedUserDataTest, DerivedClassUsesBaseRegistration) {
  UnownedUserDataHost host;
  EXPECT_EQ(nullptr, TestScopedUnownedUserData::Get(host));
  {
    DerivedWithoutDuplicateUserData derived_data(host);
    EXPECT_EQ(&derived_data, TestScopedUnownedUserData::Get(host));
  }
  EXPECT_EQ(nullptr, TestScopedUnownedUserData::Get(host));
}

TEST(UnownedUserDataTest, DifferentTypesAreIndependent) {
  UnownedUserDataHost host;
  // At the start, neither type has an associated entry on the host.
  EXPECT_EQ(nullptr, TestScopedUnownedUserData::Get(host));
  EXPECT_EQ(nullptr, TestScopedUnownedUserData2::Get(host));

  // Create an entry for the first object only.
  std::optional<TestScopedUnownedUserData> test_data1;
  test_data1.emplace(host);

  // The first will have an entry, but thesecond will not.
  EXPECT_EQ(&test_data1.value(), TestScopedUnownedUserData::Get(host));
  EXPECT_EQ(nullptr, TestScopedUnownedUserData2::Get(host));

  // Now, create an entry for the second.
  std::optional<TestScopedUnownedUserData2> test_data2;
  test_data2.emplace(host);

  // Both types should have entries.
  EXPECT_EQ(&test_data1.value(), TestScopedUnownedUserData::Get(host));
  EXPECT_EQ(&test_data2.value(), TestScopedUnownedUserData2::Get(host));

  // Unset the first. The other's entry should be unaffected.
  test_data1.reset();
  EXPECT_EQ(nullptr, TestScopedUnownedUserData::Get(host));
  EXPECT_EQ(&test_data2.value(), TestScopedUnownedUserData2::Get(host));

  // Reset the second. Both types should now be unset.
  test_data2.reset();
  EXPECT_EQ(nullptr, TestScopedUnownedUserData::Get(host));
  EXPECT_EQ(nullptr, TestScopedUnownedUserData2::Get(host));
}

// Tests that only one instance of a given type can be set on a host.
TEST(UnownedUserDataTest, CannotSetATypeTwiceOnTheSameHost) {
  UnownedUserDataHost host;
  std::optional<TestScopedUnownedUserData> test_data1;
  std::optional<TestScopedUnownedUserData> test_data2;

  // Construct one test data. This should succeed.
  test_data1.emplace(host);
  // Constructing another data of the same type should cause a CHECK failure.
  EXPECT_DEATH_IF_SUPPORTED(test_data2.emplace(host), "");
}

}  // namespace ui
