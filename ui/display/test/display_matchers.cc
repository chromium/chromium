// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/test/display_matchers.h"

namespace display {

namespace {

const float kEpsilon = 0.0001f;

// Matcher to check DisplayMode size and refresh rate.
class DisplayModeMatcher
    : public testing::MatcherInterface<const DisplayMode&> {
 public:
  DisplayModeMatcher(int width, int height, float refresh_rate)
      : size_(width, height), refresh_rate_(refresh_rate) {}

  bool MatchAndExplain(const DisplayMode& mode,
                       testing::MatchResultListener* listener) const override {
    return mode.size() == size_ &&
           std::fabs(mode.refresh_rate() - refresh_rate_) < kEpsilon;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "[" << size_.ToString() << " rate=" << refresh_rate_ << "]";
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "not [" << size_.ToString() << " rate=" << refresh_rate_ << "]";
  }

 private:
  gfx::Size size_;
  float refresh_rate_;
};

}  // namespace

testing::Matcher<const DisplayMode&> IsDisplayMode(int width,
                                                   int height,
                                                   float refresh_rate) {
  return testing::MakeMatcher(
      new DisplayModeMatcher(width, height, refresh_rate));
}

}  // namespace display
