// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test library for compression_script.
//
// The array will be put in the .data section which will be used as an argument
// for the script. We expect library to not crash and return the 55 as a
// result.

#include <numeric>
#include <vector>

#include "libtest_array.h"

extern "C" {
int GetSum();
}

int GetSum() {
  // We are using some c++ features here to better simulate a c++ library and
  // cause more code reach to catch potential memory errors.
  std::vector<int> sum_array(std::begin(array), std::end(array));
  int sum = std::accumulate(sum_array.begin(), sum_array.end(), 0);
  // sum should be equal to 1046543.
  return sum;
}
