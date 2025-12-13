// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/qt/native_theme_qt.h"

#include <cstdlib>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/color/system_theme.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/qt/qt_interface.h"

namespace qt {

NativeThemeQt::NativeThemeQt(QtInterface* shim)
    : ui::NativeThemeAura(ui::SystemTheme::kQt), shim_(shim) {}

NativeThemeQt::~NativeThemeQt() = default;

void NativeThemeQt::OnQtThemeChanged() {
  OnToolkitSettingsChanged(false);
}

DISABLE_CFI_VCALL
void NativeThemeQt::PaintFrameTopArea(
    cc::PaintCanvas* canvas,
    State state,
    const gfx::Rect& rect,
    const FrameTopAreaExtraParams& extra_params) const {
  auto image = shim_->DrawHeader(
      rect.width(), rect.height(), extra_params.default_background_color,
      extra_params.is_active ? ColorState::kNormal : ColorState::kInactive,
      extra_params.use_custom_frame);
  SkImageInfo image_info = SkImageInfo::Make(
      image.width, image.height, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  SkBitmap bitmap;
  bitmap.installPixels(
      image_info, image.data_argb.Take(), image_info.minRowBytes(),
      [](void* data, void*) { std::free(data); }, nullptr);
  bitmap.setImmutable();
  canvas->drawImage(cc::PaintImage::CreateFromBitmap(std::move(bitmap)),
                    rect.x(), rect.y());
}

}  // namespace qt
