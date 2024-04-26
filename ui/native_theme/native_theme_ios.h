// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_IOS_H_
#define UI_NATIVE_THEME_NATIVE_THEME_IOS_H_

#include "base/no_destructor.h"
#include "ui/native_theme/native_theme_base.h"

namespace ui {

// iOS implementation of native theme support.
class NativeThemeIOS : public NativeThemeBase {
 public:
  NativeThemeIOS(const NativeThemeIOS&) = delete;
  NativeThemeIOS& operator=(const NativeThemeIOS&) = delete;

  // NativeThemeBase:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;

 protected:
  friend class NativeTheme;
  friend class base::NoDestructor<NativeThemeIOS>;
  static NativeThemeIOS* instance();

  // NativeThemeBase:
  void AdjustCheckboxRadioRectForPadding(SkRect* rect) const override;
  // TODO(crbug.com/40741411): Refine hover state behavior on available pointing
  // devices.
  SkColor ControlsAccentColorForState(
      State state,
      ColorScheme color_scheme,
      const ColorProvider* color_provider) const override;
  SkColor ControlsSliderColorForState(
      State state,
      ColorScheme color_scheme,
      const ColorProvider* color_provider) const override;
  SkColor ButtonBorderColorForState(
      State state,
      ColorScheme color_scheme,
      const ColorProvider* color_provider) const override;
  SkColor ButtonFillColorForState(
      State state,
      ColorScheme color_scheme,
      const ColorProvider* color_provider) const override;
  SkColor ControlsBorderColorForState(
      State state,
      ColorScheme color_scheme,
      const ColorProvider* color_provider) const override;
  SkColor ControlsFillColorForState(
      State state,
      ColorScheme color_scheme,
      const ColorProvider* color_provider) const override;

 private:
  NativeThemeIOS();
  ~NativeThemeIOS() override;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_IOS_H_
