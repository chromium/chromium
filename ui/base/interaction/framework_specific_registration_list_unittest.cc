// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/framework_specific_registration_list.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace ui {

namespace {

class SingletonBase : public FrameworkSpecificImplementation {};

class SingletonImpl1 : public SingletonBase {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(SingletonImpl1)

class SingletonImpl2 : public SingletonBase {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(SingletonImpl2)

class SubClassImpl1 : public SingletonImpl1 {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA_SUBCLASS(SubClassImpl1, SingletonImpl1)

class SubClassImpl2 : public SingletonImpl1 {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA_SUBCLASS(SubClassImpl2, SingletonImpl1)

class SubClassImpl3 : public SingletonImpl2 {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA_SUBCLASS(SubClassImpl3, SingletonImpl2)

class SubSubClass : public SubClassImpl1 {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA_SUBCLASS(SubSubClass, SubClassImpl1)

}  // namespace

TEST(FrameworkSpecificRegistrationListTest, MaybeRegister) {
  FrameworkSpecificRegistrationList<SingletonBase> registration_list;
  EXPECT_EQ(0U, registration_list.size());

  auto* const instance = registration_list.MaybeRegister<SingletonImpl1>();
  EXPECT_NE(nullptr, instance);
  EXPECT_EQ(1U, registration_list.size());
  EXPECT_TRUE(registration_list[0].IsA<SingletonImpl1>());
}

TEST(FrameworkSpecificRegistrationListTest, MaybeRegister_Twice) {
  FrameworkSpecificRegistrationList<SingletonBase> registration_list;
  auto* const first = registration_list.MaybeRegister<SingletonImpl1>();
  auto* const second = registration_list.MaybeRegister<SingletonImpl1>();
  EXPECT_EQ(1U, registration_list.size());
  EXPECT_NE(nullptr, first);
  EXPECT_EQ(nullptr, second);
  EXPECT_TRUE(registration_list[0].IsA<SingletonImpl1>());
}

TEST(FrameworkSpecificRegistrationListTest, RegisterSubclassThenSuperclass) {
  FrameworkSpecificRegistrationList<SingletonBase> registration_list;
  auto* const first = registration_list.MaybeRegister<SubClassImpl1>();
  auto* const second = registration_list.MaybeRegister<SingletonImpl1>();
  EXPECT_EQ(1U, registration_list.size());
  EXPECT_NE(nullptr, first);
  EXPECT_EQ(nullptr, second);
  EXPECT_TRUE(registration_list[0].IsA<SingletonImpl1>());
  EXPECT_TRUE(registration_list[0].IsA<SubClassImpl1>());
}

TEST(FrameworkSpecificRegistrationListTest, MaybeRegister_TwoDifferent) {
  FrameworkSpecificRegistrationList<SingletonBase> registration_list;
  auto* const first = registration_list.MaybeRegister<SingletonImpl1>();
  auto* const second = registration_list.MaybeRegister<SingletonImpl2>();
  EXPECT_NE(static_cast<SingletonBase*>(first),
            static_cast<SingletonBase*>(second));
  EXPECT_EQ(2U, registration_list.size());
  EXPECT_TRUE(registration_list[0].IsA<SingletonImpl1>());
  EXPECT_TRUE(registration_list[1].IsA<SingletonImpl2>());
}

TEST(FrameworkSpecificRegistrationListTest, Iterator) {
  FrameworkSpecificRegistrationList<SingletonBase> registration_list;
  EXPECT_EQ(0U, registration_list.size());

  registration_list.MaybeRegister<SingletonImpl1>();
  registration_list.MaybeRegister<SingletonImpl2>();
  auto it = registration_list.begin();
  EXPECT_TRUE(it++->IsA<SingletonImpl1>());
  EXPECT_TRUE(it->IsA<SingletonImpl2>());
  EXPECT_TRUE(++it == registration_list.end());
}

TEST(FrameworkSpecificRegistrationListTest, GetImplementation) {
  FrameworkSpecificRegistrationList<SingletonBase> registration_list;
  auto* const first = registration_list.MaybeRegister<SingletonImpl1>();
  auto* const second = registration_list.MaybeRegister<SingletonImpl2>();
  auto* const i1 = registration_list.GetImplementation<SingletonImpl1>();
  EXPECT_EQ(first, i1);
  auto* const i2 = registration_list.GetImplementation<SingletonImpl2>();
  EXPECT_EQ(second, i2);
  auto* const i3 = registration_list.GetImplementation<SubSubClass>();
  EXPECT_EQ(nullptr, i3);
}

}  // namespace ui
