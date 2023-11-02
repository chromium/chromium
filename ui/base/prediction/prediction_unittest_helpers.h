// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_PREDICTION_PREDICTION_UNITTEST_HELPERS_H_
#define UI_BASE_PREDICTION_PREDICTION_UNITTEST_HELPERS_H_

#include "base/time/time.h"

namespace ui {
namespace test {

class PredictionUnittestHelpers {
 public:
  // Copied from third_party\blink\public\common\input\web_input_event.h
  static constexpr base::TimeTicks GetStaticTimeStampForTests() {
    // Note: intentionally use a relatively large delta from base::TimeTicks ==
    // 0. Otherwise, code that tracks the time ticks of the last event that
    // happened and computes a delta might get confused when the testing
    // timestamp is near 0, as the computed delta may very well be under the
    // delta threshold.
    //
    // TODO(dcheng): This really shouldn't use FromInternalValue(), but
    // constexpr support for time operations is a bit busted...
    return base::TimeTicks::FromInternalValue(123'000'000);
  }
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_PREDICTION_PREDICTION_UNITTEST_HELPERS_H_
