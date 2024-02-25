// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_layout_provider.h"

namespace views::test {

TestLayoutProvider::TestLayoutProvider() = default;
TestLayoutProvider::~TestLayoutProvider() = default;

void TestLayoutProvider::SetDistanceMetric(int metric, int value) {
  distance_metrics_[metric] = value;
}

void TestLayoutProvider::SetSnappedDialogWidth(int width) {
  snapped_dialog_width_ = width;
}

void TestLayoutProvider::SetFontDetails(
    int context,
    int style,
    const ui::ResourceBundle::FontDetails& details) {
  details_[{context, style}] = details;
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

ui::ResourceBundle::FontDetails TestLayoutProvider::GetFontDetailsImpl(
    int context,
    int style) const {
  auto it = details_.find({context, style});
  return it != details_.end()
             ? it->second
             : TypographyProvider::GetFontDetailsImpl(context, style);
}

}  // namespace views::test
