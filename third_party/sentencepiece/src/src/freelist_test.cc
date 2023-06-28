#include "src/freelist.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace sentencepiece {
namespace model {

TEST(FreeListTest, BasicTest) {
  FreeList<int> l(5);
  EXPECT_EQ(0, l.size());

  constexpr size_t kSize = 32;

  for (size_t i = 0; i < kSize; ++i) {
    int *n = l.Allocate();
    EXPECT_EQ(0, *n);
    *n = i;
  }

  EXPECT_EQ(kSize, l.size());
  for (size_t i = 0; i < kSize; ++i) {
    EXPECT_EQ(i, *l[i]);
  }

  l.Free();
  EXPECT_EQ(0, l.size());

  // Zero-initialized after `Free`.
  for (size_t i = 0; i < kSize; ++i) {
    int *n = l.Allocate();
    EXPECT_EQ(0, *n);
  }
}
}  // namespace model
}  // namespace sentencepiece
