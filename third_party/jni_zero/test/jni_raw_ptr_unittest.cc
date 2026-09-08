// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/jni_raw_ptr.h"

#include <type_traits>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace jni_zero {
namespace {

class SampleClass {};

// jni_raw_ptr.h documents JniRawPtr<T> as a zero-cost, trivially copyable
// wrapper around T*. Keep that contract enforced.
static_assert(std::is_trivially_copyable_v<JniRawPtr<SampleClass>>);
static_assert(sizeof(JniRawPtr<SampleClass>) == sizeof(SampleClass*));

}  // namespace

TEST(JniRawPtrTest, BasicCreation) {
  SampleClass sample;
  JniRawPtr<SampleClass> ptr(&sample);
  EXPECT_EQ(&sample, ptr.get());

  auto ptr_makeraw = MakeRaw(&sample);
  EXPECT_EQ(&sample, ptr_makeraw.get());
}

TEST(JniRawPtrTest, CopyAndMove) {
  SampleClass sample;
  JniRawPtr<SampleClass> ptr1(&sample);

  // Copy
  auto ptr2 = ptr1;
  EXPECT_EQ(&sample, ptr2.get());

  // Move
  auto ptr3 = std::move(ptr1);
  EXPECT_EQ(&sample, ptr3.get());

  // Copy assignment
  JniRawPtr<SampleClass> ptr4(nullptr);
  ptr4 = ptr2;
  EXPECT_EQ(&sample, ptr4.get());

  // Move assignment
  JniRawPtr<SampleClass> ptr5(nullptr);
  ptr5 = std::move(ptr3);
  EXPECT_EQ(&sample, ptr5.get());
}

TEST(JniRawPtrTest, NullPointer) {
  JniRawPtr<SampleClass> ptr(nullptr);
  EXPECT_EQ(nullptr, ptr.get());

  JniRawPtr<SampleClass> default_ptr;
  EXPECT_EQ(nullptr, default_ptr.get());
  EXPECT_FALSE(default_ptr);

  JniRawPtr<SampleClass> null_assigned = nullptr;
  EXPECT_EQ(nullptr, null_assigned.get());
}

TEST(JniRawPtrTest, DereferenceAndBool) {
  SampleClass sample;
  JniRawPtr<SampleClass> ptr(&sample);
  EXPECT_TRUE(ptr);
  EXPECT_EQ(&sample, &(*ptr));
  EXPECT_EQ(&sample, ptr.operator->());

  JniRawPtr<SampleClass> null_ptr(nullptr);
  EXPECT_FALSE(null_ptr);
}

}  // namespace jni_zero
