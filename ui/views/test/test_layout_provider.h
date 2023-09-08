// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_TEST_LAYOUT_PROVIDER_H_
#define UI_VIEWS_TEST_TEST_LAYOUT_PROVIDER_H_

#include <map>
#include <utility>

#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography_provider.h"

namespace views::test {

// Helper to test LayoutProvider overrides.
class TestLayoutProvider : public LayoutProvider, public TypographyProvider {
 public:
  TestLayoutProvider();

  TestLayoutProvider(const TestLayoutProvider&) = delete;
  TestLayoutProvider& operator=(const TestLayoutProvider&) = delete;

  ~TestLayoutProvider() override;

  // Override requests for the |metric| DistanceMetric to return |value| rather
  // than the default.
  void SetDistanceMetric(int metric, int value);

  // Override the return value of GetSnappedDialogWidth().
  void SetSnappedDialogWidth(int width);

  // Override the font details for a given |context| and |style|.
  void SetFontDetails(int context,
                      int style,
                      const ui::ResourceBundle::FontDetails& details);

  // LayoutProvider:
  int GetDistanceMetric(int metric) const override;
  const TypographyProvider& GetTypographyProvider() const override;
  int GetSnappedDialogWidth(int min_width) const override;

  // TypographyProvider:
  ui::ResourceBundle::FontDetails GetFontDetailsImpl(int context,
                                                     int style) const override;

 private:
  std::map<int, int> distance_metrics_;
  std::map<std::pair<int, int>, ui::ResourceBundle::FontDetails> details_;
  int snapped_dialog_width_ = 0;
};

}  // namespace views::test

#endif  // UI_VIEWS_TEST_TEST_LAYOUT_PROVIDER_H_
