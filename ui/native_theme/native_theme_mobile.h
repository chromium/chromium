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
                        const ExtraParams& extra) const override;

 protected:
  friend class NativeTheme;
  friend class base::NoDestructor<NativeThemeMobile>;
  static NativeThemeMobile* instance();

  // NativeThemeBase:
  void AdjustCheckboxRadioRectForPadding(SkRect* rect) const override;

 private:
  NativeThemeMobile();
  ~NativeThemeMobile() override;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_MOBILE_H_
