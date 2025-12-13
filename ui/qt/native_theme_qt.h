// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_QT_NATIVE_THEME_QT_H_
#define UI_QT_NATIVE_THEME_QT_H_

#include "base/memory/raw_ptr.h"
#include "ui/native_theme/native_theme_aura.h"

namespace qt {

class QtInterface;

class NativeThemeQt : public ui::NativeThemeAura {
 public:
  explicit NativeThemeQt(QtInterface* shim);
  NativeThemeQt(const NativeThemeQt&) = delete;
  NativeThemeQt& operator=(const NativeThemeQt&) = delete;
  ~NativeThemeQt() override;

  // Updates toolkit-related settings.
  void OnQtThemeChanged();

 protected:
  // ui::NativeThemeAura:
  void PaintFrameTopArea(
      cc::PaintCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const FrameTopAreaExtraParams& extra_params) const override;

 private:
  // IMPORTANT NOTE: All members that use `shim_` must be decorated with
  // `DISABLE_CFI_VCALL`.
  raw_ptr<QtInterface> shim_;
};

}  // namespace qt

#endif  // UI_QT_NATIVE_THEME_QT_H_
