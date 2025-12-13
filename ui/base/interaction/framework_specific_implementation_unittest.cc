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

TEST(FrameworkSpecificImplementationTest, SubclassIsA) {
  SubClassImpl1 sub1;
  SubClassImpl2 sub2;
  SubClassImpl3 sub3;
  SubSubClass subsub;

  EXPECT_TRUE(sub1.IsA<SingletonImpl1>());
  EXPECT_FALSE(sub1.IsA<SingletonImpl2>());
  EXPECT_TRUE(sub1.IsA<SubClassImpl1>());
  EXPECT_FALSE(sub1.IsA<SubClassImpl2>());
  EXPECT_FALSE(sub1.IsA<SubClassImpl3>());
  EXPECT_FALSE(sub1.IsA<SubSubClass>());

  EXPECT_TRUE(sub2.IsA<SingletonImpl1>());
  EXPECT_FALSE(sub2.IsA<SingletonImpl2>());
  EXPECT_FALSE(sub2.IsA<SubClassImpl1>());
  EXPECT_TRUE(sub2.IsA<SubClassImpl2>());
  EXPECT_FALSE(sub2.IsA<SubClassImpl3>());
  EXPECT_FALSE(sub3.IsA<SubSubClass>());

  EXPECT_FALSE(sub3.IsA<SingletonImpl1>());
  EXPECT_TRUE(sub3.IsA<SingletonImpl2>());
  EXPECT_FALSE(sub3.IsA<SubClassImpl1>());
  EXPECT_FALSE(sub3.IsA<SubClassImpl2>());
  EXPECT_TRUE(sub3.IsA<SubClassImpl3>());
  EXPECT_FALSE(sub3.IsA<SubSubClass>());

  EXPECT_TRUE(subsub.IsA<SingletonImpl1>());
  EXPECT_FALSE(subsub.IsA<SingletonImpl2>());
  EXPECT_TRUE(subsub.IsA<SubClassImpl1>());
  EXPECT_FALSE(subsub.IsA<SubClassImpl2>());
  EXPECT_FALSE(subsub.IsA<SubClassImpl3>());
  EXPECT_TRUE(subsub.IsA<SubSubClass>());
}

TEST(FrameworkSpecificImplementationTest, SubclassAsA) {
  SubClassImpl1 sub1;
  SubClassImpl2 sub2;
  SubClassImpl3 sub3;
  SubSubClass subsub;

  EXPECT_EQ(&sub1, sub1.AsA<SingletonImpl1>());
  EXPECT_EQ(nullptr, sub1.AsA<SingletonImpl2>());
  EXPECT_EQ(&sub1, sub1.AsA<SubClassImpl1>());
  EXPECT_EQ(nullptr, sub1.AsA<SubClassImpl2>());
  EXPECT_EQ(nullptr, sub1.AsA<SubClassImpl3>());
  EXPECT_EQ(nullptr, sub1.AsA<SubSubClass>());

  EXPECT_EQ(&sub2, sub2.AsA<SingletonImpl1>());
  EXPECT_EQ(nullptr, sub2.AsA<SingletonImpl2>());
  EXPECT_EQ(nullptr, sub2.AsA<SubClassImpl1>());
  EXPECT_EQ(&sub2, sub2.AsA<SubClassImpl2>());
  EXPECT_EQ(nullptr, sub2.AsA<SubClassImpl3>());
  EXPECT_EQ(nullptr, sub3.AsA<SubSubClass>());

  EXPECT_EQ(nullptr, sub3.AsA<SingletonImpl1>());
  EXPECT_EQ(&sub3, sub3.AsA<SingletonImpl2>());
  EXPECT_EQ(nullptr, sub3.AsA<SubClassImpl1>());
  EXPECT_EQ(nullptr, sub3.AsA<SubClassImpl2>());
  EXPECT_EQ(&sub3, sub3.AsA<SubClassImpl3>());
  EXPECT_EQ(nullptr, sub3.AsA<SubSubClass>());

  EXPECT_EQ(&subsub, subsub.AsA<SingletonImpl1>());
  EXPECT_EQ(nullptr, subsub.AsA<SingletonImpl2>());
  EXPECT_EQ(&subsub, subsub.AsA<SubClassImpl1>());
  EXPECT_EQ(nullptr, subsub.AsA<SubClassImpl2>());
  EXPECT_EQ(nullptr, subsub.AsA<SubClassImpl3>());
  EXPECT_EQ(&subsub, subsub.AsA<SubSubClass>());
}

}  // namespace ui
