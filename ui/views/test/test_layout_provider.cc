// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_layout_provider.h"

#include "ui/gfx/font_list.h"

namespace views {
namespace test {

TestLayoutProvider::TestLayoutProvider() = default;
TestLayoutProvider::~TestLayoutProvider() = default;

void TestLayoutProvider::SetDistanceMetric(int metric, int value) {
  distance_metrics_[metric] = value;
}

void TestLayoutProvider::SetSnappedDialogWidth(int width) {
  snapped_dialog_width_ = width;
}

void TestLayoutProvider::SetFont(int context,
                                 int style,
                                 const gfx::FontList& font) {
  fonts_[{context, style}] = font;
}

int TestLayoutProvider::GetDistanceMetric(int metric) const {
  if (distance_metrics_.count(metric))
    return distance_metrics_.find(metric)->second;
  return LayoutProvider::GetDistanceMetric(metric);
}

const TypographyProvider& TestLayoutProvider::GetTypographyProvider() const {
  return *this;
}

int TestLayoutProvider::GetSnappedDialogWidth(int min_width) const {
  return snapped_dialog_width_ ? snapped_dialog_width_ : min_width;
}

const gfx::FontList& TestLayoutProvider::GetFont(int context, int style) const {
  auto it = fonts_.find({context, style});
  return it != fonts_.end() ? it->second
                            : TypographyProvider::GetFont(context, style);
}

}  // namespace test
}  // namespace views
