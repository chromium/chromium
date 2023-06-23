// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/unique_ptr_comparator.h"

#include <memory>
#include <set>

#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz {
namespace {

class Foo {
 public:
  Foo() { instance_count++; }
  ~Foo() { instance_count--; }
  static int instance_count;
};

int Foo::instance_count = 0;

TEST(UniquePtrComparatorTest, Basic) {
  std::set<std::unique_ptr<Foo>, UniquePtrComparator> set;
  Foo* foo1 = new Foo();
  Foo* foo2 = new Foo();
  Foo* foo3 = new Foo();
  EXPECT_EQ(3, Foo::instance_count);

  set.emplace(foo1);
  set.emplace(foo2);

  auto it1 = set.find(foo1);
  EXPECT_TRUE(it1 != set.end());
  EXPECT_EQ(foo1, it1->get());

  {
    auto it2 = set.find(foo2);
    EXPECT_TRUE(it2 != set.end());
    EXPECT_EQ(foo2, it2->get());
  }

  EXPECT_TRUE(set.find(foo3) == set.end());

  set.erase(it1);
  EXPECT_EQ(2, Foo::instance_count);

  EXPECT_TRUE(set.find(foo1) == set.end());

  {
    auto it2 = set.find(foo2);
    EXPECT_TRUE(it2 != set.end());
    EXPECT_EQ(foo2, it2->get());
  }

  set.clear();
  EXPECT_EQ(1, Foo::instance_count);

  EXPECT_TRUE(set.find(foo1) == set.end());
  EXPECT_TRUE(set.find(foo2) == set.end());
  EXPECT_TRUE(set.find(foo3) == set.end());

  delete foo3;
  EXPECT_EQ(0, Foo::instance_count);
}

}  // namespace
}  // namespace ipcz
