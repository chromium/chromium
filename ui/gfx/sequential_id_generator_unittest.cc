// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/sequential_id_generator.h"

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

typedef testing::Test SequentialIDGeneratorTest;

TEST(SequentialIDGeneratorTest, RemoveMultipleNumbers) {
  const uint32_t kMinID = 4;
  SequentialIDGenerator generator(kMinID);

  EXPECT_EQ(4U, generator.GetGeneratedID(45));
  EXPECT_EQ(5U, generator.GetGeneratedID(55));
  EXPECT_EQ(6U, generator.GetGeneratedID(15));

  generator.ReleaseNumber(45);
  EXPECT_FALSE(generator.HasGeneratedIDFor(45));
  generator.ReleaseNumber(15);
  EXPECT_FALSE(generator.HasGeneratedIDFor(15));

  EXPECT_EQ(5U, generator.GetGeneratedID(55));
  EXPECT_EQ(4U, generator.GetGeneratedID(12));

  generator.ReleaseNumber(12);
  generator.ReleaseNumber(55);
  EXPECT_EQ(4U, generator.GetGeneratedID(0));
}

TEST(SequentialIDGeneratorTest, MaybeRemoveNumbers) {
  const uint32_t kMinID = 0;
  SequentialIDGenerator generator(kMinID);

  EXPECT_EQ(0U, generator.GetGeneratedID(42));

  generator.ReleaseNumber(42);
  EXPECT_FALSE(generator.HasGeneratedIDFor(42));
  generator.ReleaseNumber(42);
}

}  // namespace ui
