// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/typed_data_collection.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/scoped_typed_data.h"
#include "ui/base/interaction/typed_identifier.h"

namespace ui {

DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(int, kIntegerData);
DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(std::string, kStringData);
DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(raw_ptr<std::string>, kReferenceData);

TEST(OwnedTypedDataCollectionTest, ConstructInsertDestruct) {
  OwnedTypedDataCollection coll;
  EXPECT_EQ(0U, coll.size());
  EXPECT_TRUE(coll.empty());
  int& int_value =
      coll.Insert(std::make_unique<TypedData<int>>(kIntegerData, 2));
  EXPECT_EQ(1U, coll.size());
  EXPECT_FALSE(coll.empty());
  EXPECT_EQ(2, int_value);
  EXPECT_EQ(2, coll[kIntegerData]);
  std::string& string_value =
      coll.Insert(TypedData<std::string>(kStringData, "foo"));
  EXPECT_EQ(2U, coll.size());
  EXPECT_EQ("foo", string_value);
  EXPECT_EQ("foo", coll[kStringData]);
  auto& ref_value = coll.Emplace(kReferenceData, &string_value);
  EXPECT_EQ(3U, coll.size());
  EXPECT_EQ(&string_value, ref_value.get());
  EXPECT_EQ(&string_value, coll[kReferenceData].get());
}

TEST(OwnedTypedDataCollectionTest, GetIfPresent) {
  OwnedTypedDataCollection coll;
  EXPECT_EQ(nullptr, coll.GetIfPresent(kIntegerData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kStringData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kReferenceData));
  int& int_value =
      coll.Insert(std::make_unique<TypedData<int>>(kIntegerData, 2));
  EXPECT_EQ(&int_value, coll.GetIfPresent(kIntegerData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kStringData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kReferenceData));
  std::string& string_value =
      coll.Insert(TypedData<std::string>(kStringData, "foo"));
  EXPECT_EQ(&int_value, coll.GetIfPresent(kIntegerData));
  EXPECT_EQ(&string_value, coll.GetIfPresent(kStringData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kReferenceData));
  auto& ref_value = coll.Emplace(kReferenceData, &string_value);
  EXPECT_EQ(&int_value, coll.GetIfPresent(kIntegerData));
  EXPECT_EQ(&string_value, coll.GetIfPresent(kStringData));
  EXPECT_EQ(&ref_value, coll.GetIfPresent(kReferenceData));
}

TEST(OwnedTypedDataCollectionTest, Contains) {
  OwnedTypedDataCollection coll;
  EXPECT_FALSE(coll.Contains(kIntegerData));
  EXPECT_FALSE(coll.Contains(kIntegerData.identifier()));
  EXPECT_FALSE(coll.Contains(kStringData));
  EXPECT_FALSE(coll.Contains(kReferenceData));
  coll.Insert(std::make_unique<TypedData<int>>(kIntegerData, 2));
  EXPECT_TRUE(coll.Contains(kIntegerData));
  EXPECT_TRUE(coll.Contains(kIntegerData.identifier()));
  EXPECT_FALSE(coll.Contains(kStringData));
  EXPECT_FALSE(coll.Contains(kReferenceData));
  std::string& string_value =
      coll.Insert(TypedData<std::string>(kStringData, "foo"));
  EXPECT_TRUE(coll.Contains(kIntegerData));
  EXPECT_TRUE(coll.Contains(kStringData));
  EXPECT_FALSE(coll.Contains(kReferenceData));
  coll.Emplace(kReferenceData, &string_value);
  EXPECT_TRUE(coll.Contains(kIntegerData));
  EXPECT_TRUE(coll.Contains(kStringData));
  EXPECT_TRUE(coll.Contains(kReferenceData));
}

TEST(OwnedTypedDataCollectionTest, Clear) {
  OwnedTypedDataCollection coll;
  coll.Insert(std::make_unique<TypedData<int>>(kIntegerData, 2));
  std::string& string_value =
      coll.Insert(TypedData<std::string>(kStringData, "foo"));
  coll.Emplace(kReferenceData, &string_value);
  coll.FreeAll();
  EXPECT_EQ(0U, coll.size());
  EXPECT_TRUE(coll.empty());
  EXPECT_EQ(nullptr, coll.GetIfPresent(kIntegerData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kStringData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kReferenceData));
}

TEST(OwnedTypedDataCollectionTest, ClearAndReAdd) {
  OwnedTypedDataCollection coll;
  coll.Insert(std::make_unique<TypedData<int>>(kIntegerData, 2));
  std::string& string_value =
      coll.Insert(TypedData<std::string>(kStringData, "foo"));
  coll.Emplace(kReferenceData, &string_value);
  coll.FreeAll();

  coll.Insert(std::make_unique<TypedData<int>>(kIntegerData, 3));
  EXPECT_EQ(1U, coll.size());
  EXPECT_EQ(3, coll[kIntegerData]);
}

TEST(OwnedTypedDataCollectionTest, InsertOrAssign) {
  DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(std::unique_ptr<int>, kPtrData);

  OwnedTypedDataCollection coll;
  coll.InsertOrAssign(kIntegerData, 2);
  EXPECT_EQ(2, coll[kIntegerData]);
  coll.InsertOrAssign(kIntegerData, 5);
  EXPECT_EQ(5, coll[kIntegerData]);
  coll.InsertOrAssign(kStringData, "something");
  EXPECT_EQ("something", coll[kStringData]);
  coll.InsertOrAssign(kStringData, "something else");
  EXPECT_EQ("something else", coll[kStringData]);
  coll.InsertOrAssign(kPtrData, std::make_unique<int>(3));
  EXPECT_EQ(3, *coll[kPtrData]);
  coll.InsertOrAssign(kPtrData, std::make_unique<int>(-1));
  EXPECT_EQ(-1, *coll[kPtrData]);
}

TEST(OwnedTypedDataCollectionTest, MoveConstructor) {
  OwnedTypedDataCollection coll;
  int& int_value =
      coll.Insert(std::make_unique<TypedData<int>>(kIntegerData, 2));
  std::string& string_value =
      coll.Insert(TypedData<std::string>(kStringData, "foo"));
  auto& ref_value = coll.Emplace(kReferenceData, &string_value);

  OwnedTypedDataCollection coll2(std::move(coll));

  EXPECT_TRUE(coll.empty());
  EXPECT_EQ(nullptr, coll.GetIfPresent(kIntegerData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kStringData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kReferenceData));

  EXPECT_EQ(3U, coll2.size());
  EXPECT_EQ(&int_value, coll2.GetIfPresent(kIntegerData));
  EXPECT_EQ(&string_value, coll2.GetIfPresent(kStringData));
  EXPECT_EQ(&ref_value, coll2.GetIfPresent(kReferenceData));
}

TEST(OwnedTypedDataCollectionTest, MoveAssignment) {
  OwnedTypedDataCollection coll;
  int& int_value =
      coll.Insert(std::make_unique<TypedData<int>>(kIntegerData, 2));
  std::string& string_value =
      coll.Insert(TypedData<std::string>(kStringData, "foo"));

  OwnedTypedDataCollection coll2;
  coll2.Emplace(kReferenceData, &string_value);

  coll2 = std::move(coll);

  EXPECT_EQ(2U, coll2.size());
  EXPECT_EQ(&int_value, coll2.GetIfPresent(kIntegerData));
  EXPECT_EQ(&string_value, coll2.GetIfPresent(kStringData));
  EXPECT_EQ(nullptr, coll2.GetIfPresent(kReferenceData));
}

TEST(OwnedTypedDataCollectionTest, Append) {
  OwnedTypedDataCollection coll;
  std::string& string_value =
      coll.Insert(TypedData<std::string>(kStringData, "foo"));
  coll.Emplace(kReferenceData, &string_value);

  OwnedTypedDataCollection coll2;
  int& int_value =
      coll2.Insert(std::make_unique<TypedData<int>>(kIntegerData, 2));

  coll2.Append(std::move(coll));

  EXPECT_TRUE(coll.empty());
  EXPECT_EQ(nullptr, coll.GetIfPresent(kIntegerData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kStringData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kReferenceData));

  EXPECT_EQ(3U, coll2.size());
  EXPECT_EQ(&int_value, coll2.GetIfPresent(kIntegerData));
  EXPECT_EQ(&string_value, coll2.GetIfPresent(kStringData));
  EXPECT_NE(nullptr, coll2.GetIfPresent(kReferenceData));
}

TEST(OwnedTypedDataCollectionTest, LookupFailsIfNotPresent) {
  OwnedTypedDataCollection coll;
  coll.Insert(TypedData<std::string>(kStringData, "foo"));
  coll[kStringData];
  EXPECT_CHECK_DEATH(coll[kIntegerData]);
}

TEST(OwnedTypedDataCollectionTest, AddDuplicateDataFails) {
  OwnedTypedDataCollection coll;
  coll.Insert(TypedData<std::string>(kStringData, "foo"));
  EXPECT_CHECK_DEATH(coll.Insert(TypedData<std::string>(kStringData, "bar")));
}

TEST(OwnedTypedDataCollectionTest, AppendDuplicateDataFails) {
  OwnedTypedDataCollection coll;
  OwnedTypedDataCollection coll2;
  coll.Insert(TypedData<std::string>(kStringData, "foo"));
  coll2.Insert(TypedData<std::string>(kStringData, "bar"));
  EXPECT_CHECK_DEATH(coll2.Append(std::move(coll)));
}

TEST(OwnedTypedDataCollectionTest, ChangeValue) {
  OwnedTypedDataCollection coll;
  coll.Emplace(kStringData, "foo");
  coll[kStringData] = "bar";
  EXPECT_EQ("bar", coll[kStringData]);
}

class UnownedTypedDataCollectionTest : public testing::Test {
 public:
  UnownedTypedDataCollectionTest() = default;
  ~UnownedTypedDataCollectionTest() override = default;

  void SetUp() override {
    owned_.Emplace(kIntegerData, 2);
    std::string& string_data = owned_.Emplace(kStringData, "foo");
    owned_.Emplace(kReferenceData, &string_data);
  }

  void TearDown() override { owned_.FreeAll(); }

 protected:
  OwnedTypedDataCollection owned_;
};

TEST_F(UnownedTypedDataCollectionTest, Construct) {
  UnownedTypedDataCollection coll;
  EXPECT_TRUE(coll.empty());
  EXPECT_EQ(0U, coll.size());
  EXPECT_FALSE(coll.contains(kIntegerData.identifier()));
  EXPECT_FALSE(coll.contains(kIntegerData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kStringData));
}

TEST_F(UnownedTypedDataCollectionTest, ConstructFromOwned) {
  UnownedTypedDataCollection coll(owned_);
  EXPECT_FALSE(coll.empty());
  EXPECT_EQ(3U, coll.size());
  EXPECT_TRUE(coll.contains(kIntegerData));
  EXPECT_TRUE(coll.contains(kIntegerData.identifier()));
  EXPECT_EQ(owned_.GetIfPresent(kStringData), coll.GetIfPresent(kStringData));
}

TEST_F(UnownedTypedDataCollectionTest, Add) {
  UnownedTypedDataCollection coll;
  TypedData int_data(kIntegerData, 3);
  TypedData string_data(kStringData, "bar");
  EXPECT_FALSE(coll.contains(kIntegerData));
  coll.Add(int_data);
  EXPECT_EQ(3, coll[kIntegerData]);
  coll.Add(string_data);
  EXPECT_EQ("bar", coll[kStringData]);
  EXPECT_EQ(2U, coll.size());
}

TEST_F(UnownedTypedDataCollectionTest, AddAll) {
  UnownedTypedDataCollection coll;
  coll.AddAll(owned_);
  EXPECT_FALSE(coll.empty());
  EXPECT_EQ(3U, coll.size());
  EXPECT_TRUE(coll.contains(kIntegerData));
  EXPECT_EQ(owned_.GetIfPresent(kStringData), coll.GetIfPresent(kStringData));
}

TEST_F(UnownedTypedDataCollectionTest, AddFrom) {
  UnownedTypedDataCollection coll;
  coll.AddFrom(kIntegerData, owned_);
  EXPECT_FALSE(coll.empty());
  EXPECT_EQ(1U, coll.size());
  EXPECT_TRUE(coll.contains(kIntegerData));
  EXPECT_FALSE(coll.contains(kStringData));
  coll.AddFrom(kStringData, owned_);
  EXPECT_FALSE(coll.empty());
  EXPECT_EQ(2U, coll.size());
  EXPECT_TRUE(coll.contains(kIntegerData));
  EXPECT_TRUE(coll.contains(kStringData));
}

TEST_F(UnownedTypedDataCollectionTest, AddSameDataSucceeds) {
  UnownedTypedDataCollection coll;
  coll.AddFrom(kIntegerData, owned_);
  coll.AddFrom(kIntegerData, owned_);
  EXPECT_FALSE(coll.empty());
  EXPECT_EQ(1U, coll.size());
  EXPECT_TRUE(coll.contains(kIntegerData));
}

TEST_F(UnownedTypedDataCollectionTest, AddDuplicateIdFails) {
  OwnedTypedDataCollection owned2;
  owned2.Emplace(kIntegerData, 3);
  UnownedTypedDataCollection coll;
  coll.AddFrom(kIntegerData, owned_);
  EXPECT_CHECK_DEATH(coll.AddFrom(kIntegerData, owned2));
}

TEST_F(UnownedTypedDataCollectionTest, AddAllWithDuplicateIdsFails) {
  OwnedTypedDataCollection owned2;
  owned2.Emplace(kIntegerData, 3);
  UnownedTypedDataCollection coll;
  coll.AddAll(owned_);
  EXPECT_CHECK_DEATH(coll.AddAll(owned2));
}

TEST_F(UnownedTypedDataCollectionTest, ReleaseReferences) {
  UnownedTypedDataCollection coll;
  coll.AddAll(owned_);
  coll.ReleaseAllReferences();
  owned_.FreeAll();
}

TEST_F(UnownedTypedDataCollectionTest, ScopedDataForTesting) {
  DEFINE_LOCAL_TYPED_IDENTIFIER_VALUE(float, kFloatData);
  UnownedTypedDataCollection coll;
  {
    test::ScopedTypedData float_data(coll, kFloatData, 4.2f);
    EXPECT_EQ(4.2f, coll[kFloatData]);
    *float_data = 1.1f;
    EXPECT_EQ(1.1f, coll[kFloatData]);
  }
  EXPECT_FALSE(coll.contains(kFloatData));
}

TEST_F(UnownedTypedDataCollectionTest,
       ScopedDataForTestingReplacesAndRestoresExistingValue) {
  UnownedTypedDataCollection coll(owned_);
  {
    test::ScopedTypedData int_data(coll, kIntegerData, 4);
    EXPECT_EQ(4, coll[kIntegerData]);
  }
  EXPECT_EQ(2, coll[kIntegerData]);
}

TEST_F(UnownedTypedDataCollectionTest, GetIfPresent) {
  UnownedTypedDataCollection coll;
  EXPECT_EQ(nullptr, coll.GetIfPresent(kIntegerData));
  test::ScopedTypedData int_data(coll, kIntegerData, 3);
  EXPECT_EQ(int_data.get(), coll.GetIfPresent(kIntegerData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kStringData));
  {
    test::ScopedTypedData string_data(coll, kStringData, "bar");
    EXPECT_EQ(int_data.get(), coll.GetIfPresent(kIntegerData));
    EXPECT_EQ(string_data.get(), coll.GetIfPresent(kStringData));
  }
  EXPECT_EQ(int_data.get(), coll.GetIfPresent(kIntegerData));
  EXPECT_EQ(nullptr, coll.GetIfPresent(kStringData));
}

TEST_F(UnownedTypedDataCollectionTest, Lookup) {
  UnownedTypedDataCollection coll;
  test::ScopedTypedData int_data(coll, kIntegerData, 3);
  EXPECT_EQ(3, coll[kIntegerData]);
  {
    test::ScopedTypedData string_data(coll, kStringData, "bar");
    EXPECT_EQ(3, coll[kIntegerData]);
    EXPECT_EQ("bar", coll[kStringData]);
    coll[kIntegerData] = 4;
    EXPECT_EQ(4, coll[kIntegerData]);
  }
  EXPECT_EQ(4, coll[kIntegerData]);
}

TEST_F(UnownedTypedDataCollectionTest, LookupFails) {
  UnownedTypedDataCollection coll;
  test::ScopedTypedData int_data(coll, kIntegerData, 3);
  EXPECT_CHECK_DEATH(coll[kStringData]);
}

}  // namespace ui
