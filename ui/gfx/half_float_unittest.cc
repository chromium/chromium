// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/half_float.h"

#include <math.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

class HalfFloatTest : public testing::Test {
 public:
  union FloatUIntUnion {
    // this must come first for the initializations below to work
    uint32_t fUInt;
    float fFloat;
  };

  // Convert an IEEE 754 half-float to a float value
  // that we can do math on.
  float FromHalfFloat(HalfFloat half_float) {
    int sign = (half_float & 0x8000) ? -1 : 1;
    int exponent = (half_float >> 10) & 0x1F;
    int fraction = half_float & 0x3FF;
    if (exponent == 0) {
      return powf(2.0f, -24.0f) * fraction;
    } else if (exponent == 0x1F) {
      return sign * 1000000000000.0f;
    } else {
      return pow(2.0f, exponent - 25) * (0x400 + fraction);
    }
  }

  HalfFloat ConvertTruth(float f) {
    if (f < 0.0)
      return 0x8000 | ConvertTruth(-f);
    int max = 0x8000;
    int min = 0;
    while (max - min > 1) {
      int mid = (min + max) >> 1;
      if (FromHalfFloat(mid) > f) {
        max = mid;
      } else {
        min = mid;
      }
    }
    float low = FromHalfFloat(min);
    float high = FromHalfFloat(min + 1);
    if (f - low <= high - f) {
      return min;
    } else {
      return min + 1;
    }
  }

  HalfFloat Convert(float f) {
    HalfFloat ret;
    FloatToHalfFloat(&f, &ret, 1);
    return ret;
  }
};

TEST_F(HalfFloatTest, NoCrashTest) {
  Convert(nanf(""));
  Convert(1.0E30f);
  Convert(-1.0E30f);
  Convert(1.0E-30f);
  Convert(-1.0E-30f);
}

TEST_F(HalfFloatTest, SimpleTest) {
  static float test[] = {
      0.0f,    1.0f,    10.0f,    1000.0f,  65503.0f,
      1.0E-3f, 1.0E-6f, 1.0E-20f, 1.0E-44f,
  };
  for (size_t i = 0; i < std::size(test); i++) {
    EXPECT_EQ(ConvertTruth(test[i]), Convert(test[i])) << " float = "
                                                       << test[i];
    if (test[i] != 0.0) {
      EXPECT_EQ(ConvertTruth(-test[i]), Convert(-test[i])) << " float = "
                                                           << -test[i];
    }
  }
}

}  // namespace
