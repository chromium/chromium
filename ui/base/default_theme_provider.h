// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DEFAULT_THEME_PROVIDER_H_
#define UI_BASE_DEFAULT_THEME_PROVIDER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_export.h"

namespace ui {

class UI_BASE_EXPORT DefaultThemeProvider : public ThemeProvider {
 public:
  DefaultThemeProvider();
  ~DefaultThemeProvider() override;

  // Overridden from ui::ThemeProvider:
  gfx::ImageSkia* GetImageSkiaNamed(int id) const override;
  SkColor GetColor(int id) const override;
  color_utils::HSL GetTint(int id) const override;
  int GetDisplayProperty(int id) const override;
  bool ShouldUseNativeFrame() const override;
  bool HasCustomImage(int id) const override;
  bool HasCustomColor(int id) const override;
  base::RefCountedMemory* GetRawData(int id, ui::ScaleFactor scale_factor)
      const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultThemeProvider);
};

}  // namespace ui

#endif  // UI_BASE_DEFAULT_THEME_PROVIDER_H_
