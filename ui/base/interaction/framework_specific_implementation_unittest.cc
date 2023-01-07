// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/interaction/framework_specific_implementation.h"

#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace

TEST(FrameworkSpecificRegistrationListTest, MaybeRegister) {
  FrameworkSpecificRegistrationList<SingletonBase> registration_list;
  EXPECT_EQ(0U, registration_list.size());

  registration_list.MaybeRegister<SingletonImpl1>();
  EXPECT_EQ(1U, registration_list.size());
  EXPECT_TRUE(registration_list[0].IsA<SingletonImpl1>());
}

TEST(FrameworkSpecificRegistrationListTest, MaybeRegister_Twice) {
  FrameworkSpecificRegistrationList<SingletonBase> registration_list;
  registration_list.MaybeRegister<SingletonImpl1>();
  registration_list.MaybeRegister<SingletonImpl1>();
  EXPECT_EQ(1U, registration_list.size());
  EXPECT_TRUE(registration_list[0].IsA<SingletonImpl1>());
}

TEST(FrameworkSpecificRegistrationListTest, MaybeRegister_TwoDifferent) {
  FrameworkSpecificRegistrationList<SingletonBase> registration_list;
  registration_list.MaybeRegister<SingletonImpl1>();
  registration_list.MaybeRegister<SingletonImpl2>();
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

}  // namespace ui
