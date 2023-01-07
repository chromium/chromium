// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/view_prop.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kKey1[] = "key_1";
const char kKey2[] = "key_2";
}  // namespace

namespace ui {

// Test a handful of viewprop assertions.
TEST(ViewPropTest, Basic) {
  gfx::AcceleratedWidget nv1 = reinterpret_cast<gfx::AcceleratedWidget>(1);
  gfx::AcceleratedWidget nv2 = reinterpret_cast<gfx::AcceleratedWidget>(2);

  void* data1 = reinterpret_cast<void*>(11);
  void* data2 = reinterpret_cast<void*>(12);

  // Initial value for a new view/key pair should be NULL.
  EXPECT_EQ(NULL, ViewProp::GetValue(nv1, kKey1));

  {
    // Register a value for a view/key pair.
    ViewProp prop(nv1, kKey1, data1);
    EXPECT_EQ(data1, ViewProp::GetValue(nv1, kKey1));
  }

  // The property fell out of scope, so the value should now be NULL.
  EXPECT_EQ(NULL, ViewProp::GetValue(nv1, kKey1));

  {
    // Register a value for a view/key pair.
    std::unique_ptr<ViewProp> v1(new ViewProp(nv1, kKey1, data1));
    EXPECT_EQ(data1, ViewProp::GetValue(nv1, kKey1));

    // Register a value for the same view/key pair.
    std::unique_ptr<ViewProp> v2(new ViewProp(nv1, kKey1, data2));
    // The new value should take over.
    EXPECT_EQ(data2, ViewProp::GetValue(nv1, kKey1));

    // Null out the first ViewProp, which should NULL out the value.
    v1.reset(NULL);
    EXPECT_EQ(NULL, ViewProp::GetValue(nv1, kKey1));
  }

  // The property fell out of scope, so the value should now be NULL.
  EXPECT_EQ(NULL, ViewProp::GetValue(nv1, kKey1));

  {
    // Register a value for a view/key pair.
    std::unique_ptr<ViewProp> v1(new ViewProp(nv1, kKey1, data1));
    std::unique_ptr<ViewProp> v2(new ViewProp(nv2, kKey2, data2));
    EXPECT_EQ(data1, ViewProp::GetValue(nv1, kKey1));
    EXPECT_EQ(data2, ViewProp::GetValue(nv2, kKey2));

    v1.reset(NULL);
    EXPECT_EQ(NULL, ViewProp::GetValue(nv1, kKey1));
    EXPECT_EQ(data2, ViewProp::GetValue(nv2, kKey2));

    v2.reset(NULL);
    EXPECT_EQ(NULL, ViewProp::GetValue(nv1, kKey1));
    EXPECT_EQ(NULL, ViewProp::GetValue(nv2, kKey2));
  }
}

}  // namespace ui
