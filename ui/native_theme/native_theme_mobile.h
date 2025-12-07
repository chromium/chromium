// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_MOBILE_H_
#define UI_NATIVE_THEME_NATIVE_THEME_MOBILE_H_

#include "base/no_destructor.h"
#include "ui/native_theme/native_theme_base.h"

namespace ui {

class NativeThemeMobile : public NativeThemeBase {
 public:
  NativeThemeMobile(const NativeThemeMobile&) = delete;
  NativeThemeMobile& operator=(const NativeThemeMobile&) = delete;

  // NativeThemeBase:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra_params) const override;

 protected:
  // NativeThemeBase:
  SkColor GetControlColor(ControlColorId color_id,
                          bool dark_mode,
                          PreferredContrast contrast,
                          const ColorProvider* color_provider) const override;
  void PaintArrowButton(
      cc::PaintCanvas* gc,
      const ColorProvider* color_provider,
      const gfx::Rect& rect,
      Part part,
      State state,
      bool forced_colors,
      bool dark_mode,
      PreferredContrast contrast,
      const ScrollbarArrowExtraParams& extra_params) const override;

 private:
  friend class base::NoDestructor<NativeThemeMobile>;

  NativeThemeMobile();
  ~NativeThemeMobile() override;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_MOBILE_H_
