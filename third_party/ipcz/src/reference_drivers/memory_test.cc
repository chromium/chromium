// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/memory.h"

#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz::reference_drivers {
namespace {

using MemoryTest = testing::Test;

TEST_F(MemoryTest, CreateAndMap) {
  Memory memory(64);

  Memory::Mapping mapping0 = memory.Map();
  Memory::Mapping mapping1 = memory.Map();

  int* data0 = mapping0.As<int>();
  int* data1 = mapping1.As<int>();

  // Each mapping should have a different base address.
  EXPECT_NE(data0, data1);

  // But they should be backed by the same physical memory.
  data1[0] = 0;
  data0[0] = 42;
  EXPECT_EQ(42, data1[0]);
}

TEST_F(MemoryTest, CreateMapClose) {
  Memory memory(64);

  Memory::Mapping mapping0 = memory.Map();
  Memory::Mapping mapping1 = memory.Map();

  // Even with the memfd closed, the mappings above should persist.
  memory.reset();

  int* data0 = mapping0.As<int>();
  int* data1 = mapping1.As<int>();
  EXPECT_NE(data0, data1);
  data1[0] = 0;
  data0[0] = 42;
  EXPECT_EQ(42, data1[0]);
}

TEST_F(MemoryTest, CreateCloneMapClose) {
  Memory memory(64);
  Memory clone = memory.Clone();

  Memory::Mapping mapping0 = memory.Map();
  Memory::Mapping mapping1 = clone.Map();

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
