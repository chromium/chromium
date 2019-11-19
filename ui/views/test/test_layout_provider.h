// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_LAYOUT_PROVIDER_H_
#define UI_VIEWS_TEST_TEST_LAYOUT_PROVIDER_H_

#include <map>

#include "base/macros.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography_provider.h"

namespace gfx {
class FontList;
}

namespace views {
namespace test {

// Helper to test LayoutProvider overrides.
class TestLayoutProvider : public LayoutProvider, public TypographyProvider {
 public:
  TestLayoutProvider();
  ~TestLayoutProvider() override;

  // Override requests for the |metric| DistanceMetric to return |value| rather
  // than the default.
  void SetDistanceMetric(int metric, int value);

  // Override the return value of GetSnappedDialogWidth().
  void SetSnappedDialogWidth(int width);

  // Override the font provided by style::GetFont().
  void SetFont(int context, int style, const gfx::FontList& font);

  // LayoutProvider:
  int GetDistanceMetric(int metric) const override;
  const TypographyProvider& GetTypographyProvider() const override;
  int GetSnappedDialogWidth(int min_width) const override;

  // TypographyProvider:
  const gfx::FontList& GetFont(int context, int style) const override;

 private:
  std::map<int, int> distance_metrics_;
  std::map<std::pair<int, int>, gfx::FontList> fonts_;
  int snapped_dialog_width_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestLayoutProvider);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_TEST_LAYOUT_PROVIDER_H_
