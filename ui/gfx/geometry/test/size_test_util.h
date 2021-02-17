// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_TEST_SIZE_TEST_UTIL_H_
#define UI_GFX_GEOMETRY_TEST_SIZE_TEST_UTIL_H_

#define EXPECT_FLOAT_SIZE_EQ(expected, actual)               \
  do {                                                       \
    EXPECT_FLOAT_EQ((expected).width(), (actual).width());   \
    EXPECT_FLOAT_EQ((expected).height(), (actual).height()); \
  } while (false)

#define EXPECT_SIZE_EQ(expected, actual)               \
  do {                                                 \
    EXPECT_EQ((expected).width(), (actual).width());   \
    EXPECT_EQ((expected).height(), (actual).height()); \
  } while (false)

#endif  // UI_GFX_GEOMETRY_TEST_SIZE_TEST_UTIL_H_
