// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_COLOR_TRACKING_ICON_VIEW_H_
#define UI_VIEWS_CONTROLS_COLOR_TRACKING_ICON_VIEW_H_

#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/image_view.h"

namespace gfx {
struct VectorIcon;
}

namespace views {

// An ImageView that displays |icon| at |icon_size|. Tracks theme changes so the
// icon is always the correct color.
class VIEWS_EXPORT ColorTrackingIconView : public ImageView {
 public:
  ColorTrackingIconView(const gfx::VectorIcon& icon,
                        int icon_size,
                        ui::NativeTheme::ColorId icon_color_id =
                            ui::NativeTheme::kColorId_DefaultIconColor);

  // ImageView:
  void OnThemeChanged() override;

 private:
  const gfx::VectorIcon& icon_;
  const int icon_size_;
  const ui::NativeTheme::ColorId icon_color_id_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_COLOR_TRACKING_ICON_VIEW_H_
