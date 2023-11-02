// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_PREDICTION_INPUT_FILTER_UNITTEST_HELPERS_H_
#define UI_BASE_PREDICTION_INPUT_FILTER_UNITTEST_HELPERS_H_

#include "ui/base/prediction/input_filter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace test {

constexpr double kEpsilon = 0.0001;

// Base class for predictor unit tests
class InputFilterTest : public testing::Test {
 public:
  InputFilterTest();

  InputFilterTest(const InputFilterTest&) = delete;
  InputFilterTest& operator=(const InputFilterTest&) = delete;

  ~InputFilterTest() override;

  void TestCloneFilter();

  void TestResetFilter();

 protected:
  std::unique_ptr<InputFilter> filter_;
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_PREDICTION_INPUT_FILTER_UNITTEST_HELPERS_H_
