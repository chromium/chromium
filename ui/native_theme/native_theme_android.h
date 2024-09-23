// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_ANDROID_H_
#define UI_NATIVE_THEME_NATIVE_THEME_ANDROID_H_

#include "base/no_destructor.h"
#include "ui/native_theme/native_theme_base.h"

namespace ui {

// Android implementation of native theme support.
class NativeThemeAndroid : public NativeThemeBase {
 public:
  NativeThemeAndroid(const NativeThemeAndroid&) = delete;
  NativeThemeAndroid& operator=(const NativeThemeAndroid&) = delete;

  // NativeThemeBase:
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;

 protected:
  friend class NativeTheme;
  friend class base::NoDestructor<NativeThemeAndroid>;
  static NativeThemeAndroid* instance();

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
  NativeThemeAndroid();
  ~NativeThemeAndroid() override;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_ANDROID_H_
