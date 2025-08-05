// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>
#include <tuple>

int UnsafeIndex();  // This function might return an out-of-bound index.

// Mock implementation of EXPECT_EQ
#define EXPECT_EQ(lhs, rhs) IMPL_EQ(lhs, rhs)
#define IMPL_EQ(lhs, rhs) INNER_IMPL_EQ(lhs, rhs)
#define INNER_IMPL_EQ(lhs, rhs) \
  if (lhs == rhs) {             \
    /* Do nothing. */           \
  } else {                      \
    /* Do crash. */             \
  }

// EXPECT_EQ is an allow-listed macro, but CUSTOM_EQ is not.
#define CUSTOM_EQ(lhs, rhs) EXPECT_EQ(lhs, rhs)

// NO_ARG_EQ is not an allow-listed macro, nor taking a macro argument, either.
//
// Unexpected rewrite:
// #define NO_ARG_EQ() \
//         EXPECT_EQ(std::memcmp(buf.data() + 0, buf.data() + 1, 2), 0)
#define NO_ARG_EQ() EXPECT_EQ(std::memcmp(buf + 0, buf + 1, 2), 0)

void basic_test() {
  // Expected rewrite:
  // auto buf = std::to_array<int>({1, 2, 3, 4, 5});
  int buf[] = {1, 2, 3, 4, 5};
  std::ignore = buf[UnsafeIndex()];

  // The regular rewriting is applied to EXPECT_ and ASSERT_ family.
  //
  // Expected rewrite:
  // EXPECT_EQ(std::memcmp(base::span<int>(buf).subspan(0u).data(),
  //                       base::span<int>(buf).subspan(1u).data(), 2),
  //           0);
  EXPECT_EQ(std::memcmp(buf + 0, buf + 1, 2), 0);

  // The regular rewriting is NOT applied to CUSTOM_EQ although CUSTOM_EQ is
  // built on the top of EXPECT_EQ.
  //
  // Expected rewrite:
  // UNSAFE_TODO(CUSTOM_EQ(std::memcmp(buf.data() + 0, buf.data() + 1, 2), 0));
  CUSTOM_EQ(std::memcmp(buf + 0, buf + 1, 2), 0);

  // Unexpected rewrite:
  // UNSAFE_TODO(NO_ARG_EQ());
  NO_ARG_EQ();
}
