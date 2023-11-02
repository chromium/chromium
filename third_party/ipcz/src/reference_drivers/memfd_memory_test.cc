// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/memfd_memory.h"

#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz::reference_drivers {
namespace {

using MemfdMemoryTest = testing::Test;

TEST_F(MemfdMemoryTest, CreateAndMap) {
  MemfdMemory memory(64);

  MemfdMemory::Mapping mapping0 = memory.Map();
  MemfdMemory::Mapping mapping1 = memory.Map();

  int* data0 = mapping0.As<int>();
  int* data1 = mapping1.As<int>();

  // Each mapping should have a different base address.
  EXPECT_NE(data0, data1);

  // But they should be backed by the same physical memory.
  data1[0] = 0;
  data0[0] = 42;
  EXPECT_EQ(42, data1[0]);
}

TEST_F(MemfdMemoryTest, CreateMapClose) {
  MemfdMemory memory(64);

  MemfdMemory::Mapping mapping0 = memory.Map();
  MemfdMemory::Mapping mapping1 = memory.Map();

  // Even with the memfd closed, the mappings above should persist.
  memory.reset();

  int* data0 = mapping0.As<int>();
  int* data1 = mapping1.As<int>();
  EXPECT_NE(data0, data1);
  data1[0] = 0;
  data0[0] = 42;
  EXPECT_EQ(42, data1[0]);
}

TEST_F(MemfdMemoryTest, CreateCloneMapClose) {
  MemfdMemory memory(64);
  MemfdMemory clone = memory.Clone();

  MemfdMemory::Mapping mapping0 = memory.Map();
  MemfdMemory::Mapping mapping1 = clone.Map();

  memory.reset();
  clone.reset();

  int* data0 = mapping0.As<int>();
  int* data1 = mapping1.As<int>();
  EXPECT_NE(data0, data1);
  data1[0] = 0;
  data0[0] = 42;
  EXPECT_EQ(42, data1[0]);
}

}  // namespace
}  // namespace ipcz::reference_drivers
