// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/padded_button.h"

#include <memory>

#include "build/chromeos_buildflags.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"

namespace message_center {

PaddedButton::PaddedButton(PressedCallback callback)
    : views::ImageButton(std::move(callback)) {
  SetBorder(views::CreateEmptyBorder(gfx::Insets(kControlButtonBorderSize)));
  SetAnimateOnStateChange(false);

  ink_drop()->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop()->SetVisibleOpacity(0.12f);
  SetHasInkDropActionOnClick(true);
  views::InkDrop::UseInkDropForSquareRipple(ink_drop(),
                                            /*highlight_on_hover=*/false);
}

void PaddedButton::OnThemeChanged() {
  ImageButton::OnThemeChanged();
  auto* theme = GetNativeTheme();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  SkColor background_color = theme->GetSystemColor(
      ui::NativeTheme::kColorId_NotificationButtonBackground);
  SetBackground(views::CreateSolidBackground(background_color));
#else
  SkColor background_color =
      theme->GetSystemColor(ui::NativeTheme::kColorId_WindowBackground);
#endif
  ink_drop()->SetBaseColor(
      color_utils::GetColorWithMaxContrast(background_color));
}

BEGIN_METADATA(PaddedButton, views::ImageButton)
END_METADATA

}  // namespace message_center
