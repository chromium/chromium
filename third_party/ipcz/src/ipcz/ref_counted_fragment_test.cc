// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/ref_counted_fragment.h"

#include <atomic>
#include <tuple>

#include "ipcz/driver_memory.h"
#include "ipcz/fragment.h"
#include "ipcz/fragment_ref.h"
#include "ipcz/node.h"
#include "ipcz/node_link_memory.h"
#include "reference_drivers/sync_reference_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

const IpczDriver& kTestDriver = reference_drivers::kSyncReferenceDriver;

using RefCountedFragmentTest = testing::Test;

using TestObject = RefCountedFragment;

TEST_F(RefCountedFragmentTest, NullRef) {
  FragmentRef<TestObject> ref;
  EXPECT_TRUE(ref.is_null());
  EXPECT_FALSE(ref.is_addressable());

  ref.reset();
  EXPECT_TRUE(ref.is_null());
  EXPECT_FALSE(ref.is_addressable());

  FragmentRef<TestObject> other1 = ref;
  EXPECT_TRUE(ref.is_null());
  EXPECT_FALSE(ref.is_addressable());
  EXPECT_TRUE(other1.is_null());
  EXPECT_FALSE(other1.is_addressable());

  FragmentRef<TestObject> other2 = std::move(ref);
  EXPECT_TRUE(ref.is_null());
  EXPECT_FALSE(ref.is_addressable());
  EXPECT_TRUE(other2.is_null());
  EXPECT_FALSE(other2.is_addressable());

  ref = other1;
  EXPECT_TRUE(ref.is_null());
  EXPECT_FALSE(ref.is_addressable());
  EXPECT_TRUE(other1.is_null());
  EXPECT_FALSE(other1.is_addressable());

  ref = std::move(other2);
  EXPECT_TRUE(ref.is_null());
  EXPECT_FALSE(ref.is_addressable());
  EXPECT_TRUE(other1.is_null());
  EXPECT_FALSE(other1.is_addressable());
}

TEST_F(RefCountedFragmentTest, SimpleRef) {
  TestObject object;

  FragmentRef<TestObject> ref(
      RefCountedFragment::kUnmanagedRef,
      Fragment::FromDescriptorUnsafe(
          FragmentDescriptor(BufferId(0), 0, sizeof(object)), &object));
  EXPECT_EQ(1, object.ref_count_for_testing());
  ref.reset();
  EXPECT_EQ(0, object.ref_count_for_testing());
}

TEST_F(RefCountedFragmentTest, Copy) {
  TestObject object1;

  FragmentRef<TestObject> ref1(
      RefCountedFragment::kUnmanagedRef,
      Fragment::FromDescriptorUnsafe(
          FragmentDescriptor(BufferId(0), 0, sizeof(object1)), &object1));
  EXPECT_EQ(1, object1.ref_count_for_testing());

  FragmentRef<TestObject> other1 = ref1;
  EXPECT_EQ(2, object1.ref_count_for_testing());
  other1.reset();
  EXPECT_EQ(1, object1.ref_count_for_testing());
  EXPECT_TRUE(other1.is_null());
  EXPECT_FALSE(other1.is_addressable());

  TestObject object2;
  auto ref2 = FragmentRef<TestObject>(
      RefCountedFragment::kUnmanagedRef,
      Fragment::FromDescriptorUnsafe(
          FragmentDescriptor(BufferId(0), 0, sizeof(object2)), &object2));
  EXPECT_EQ(1, object1.ref_count_for_testing());
  EXPECT_EQ(1, object2.ref_count_for_testing());
  ref2 = ref1;
  EXPECT_EQ(2, object1.ref_count_for_testing());
  EXPECT_EQ(0, object2.ref_count_for_testing());
  EXPECT_FALSE(ref1.is_null());
  EXPECT_TRUE(ref1.is_addressable());
  EXPECT_FALSE(ref2.is_null());
  EXPECT_TRUE(ref2.is_addressable());
  ref1.reset();
  EXPECT_EQ(1, object1.ref_count_for_testing());
  EXPECT_EQ(0, object2.ref_count_for_testing());
  EXPECT_TRUE(ref1.is_null());
  EXPECT_FALSE(ref1.is_addressable());
  ref2.reset();
  EXPECT_EQ(0, object1.ref_count_for_testing());
  EXPECT_EQ(0, object2.ref_count_for_testing());
  EXPECT_TRUE(ref2.is_null());
  EXPECT_FALSE(ref2.is_addressable());
}

TEST_F(RefCountedFragmentTest, Move) {
  TestObject object1;

  FragmentRef<TestObject> ref1(
      RefCountedFragment::kUnmanagedRef,
      Fragment::FromDescriptorUnsafe(
          FragmentDescriptor(BufferId(0), 0, sizeof(object1)), &object1));
  EXPECT_EQ(1, ref1.ref_count_for_testing());

  FragmentRef<TestObject> other1 = std::move(ref1);
  EXPECT_EQ(1, object1.ref_count_for_testing());
  EXPECT_FALSE(other1.is_null());
  EXPECT_TRUE(other1.is_addressable());
  EXPECT_TRUE(ref1.is_null());
  EXPECT_FALSE(ref1.is_addressable());
  other1.reset();
  EXPECT_TRUE(other1.is_null());
  EXPECT_FALSE(other1.is_addressable());
  EXPECT_EQ(0, object1.ref_count_for_testing());

  TestObject object2;
  TestObject object3;
  FragmentRef<TestObject> ref2(
      RefCountedFragment::kUnmanagedRef,
      Fragment::FromDescriptorUnsafe(
          FragmentDescriptor(BufferId(0), 0, sizeof(object2)), &object2));
  FragmentRef<TestObject> ref3(
      RefCountedFragment::kUnmanagedRef,
      Fragment::FromDescriptorUnsafe(
          FragmentDescriptor(BufferId(0), 0, sizeof(object3)), &object3));

  EXPECT_FALSE(ref2.is_null());
  EXPECT_TRUE(ref2.is_addressable());
  EXPECT_FALSE(ref3.is_null());
  EXPECT_TRUE(ref3.is_addressable());
  EXPECT_EQ(1, object2.ref_count_for_testing());
  EXPECT_EQ(1, object3.ref_count_for_testing());
  ref3 = std::move(ref2);
  EXPECT_EQ(1, object2.ref_count_for_testing());
  EXPECT_EQ(0, object3.ref_count_for_testing());
  EXPECT_TRUE(ref2.is_null());
  EXPECT_FALSE(ref2.is_addressable());
  EXPECT_FALSE(ref3.is_null());
  EXPECT_TRUE(ref3.is_addressable());
  ref3.reset();
  EXPECT_TRUE(ref3.is_null());
  EXPECT_FALSE(ref3.is_addressable());
  EXPECT_EQ(0, object2.ref_count_for_testing());
  EXPECT_EQ(0, object3.ref_count_for_testing());
}

TEST_F(RefCountedFragmentTest, Free) {
  auto node = MakeRefCounted<Node>(Node::Type::kNormal, kTestDriver);
  DriverMemoryWithMapping buffer = NodeLinkMemory::AllocateMemory(kTestDriver);
  auto memory = NodeLinkMemory::Create(std::move(node), LinkSide::kA,
                                       Features{}, std::move(buffer.mapping));

  // Allocate a ton of fragments and let them be released by FragmentRef on
  // destruction. If the fragments aren't freed properly, allocations will fail
  // and so will the test.
  constexpr size_t kNumAllocations = 100000;
  for (size_t i = 0; i < kNumAllocations; ++i) {
    Fragment fragment = memory->AllocateFragment(sizeof(TestObject));
    EXPECT_TRUE(fragment.is_addressable());
    FragmentRef<TestObject> ref(kAdoptExistingRef, memory, fragment);
  }
}

}  // namespace
}  // namespace ipcz
