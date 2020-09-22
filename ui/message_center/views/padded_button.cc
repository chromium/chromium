// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/padded_button.h"

#include <memory>

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/painter.h"

namespace message_center {

PaddedButton::PaddedButton(views::ButtonListener* listener)
    : views::ImageButton(listener) {
  SetFocusForPlatform();
  SetBorder(views::CreateEmptyBorder(gfx::Insets(kControlButtonBorderSize)));
  SetAnimateOnStateChange(false);

  SetInkDropMode(InkDropMode::ON);
  SetInkDropVisibleOpacity(0.12f);
  SetHasInkDropActionOnClick(true);
}

std::unique_ptr<views::InkDrop> PaddedButton::CreateInkDrop() {
  auto ink_drop = CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  ink_drop->SetShowHighlightOnFocus(false);
  return std::move(ink_drop);
}

void PaddedButton::OnThemeChanged() {
  ImageButton::OnThemeChanged();
  auto* theme = GetNativeTheme();
#if defined(OS_CHROMEOS)
  SetBackground(views::CreateSolidBackground(theme->GetSystemColor(
      ui::NativeTheme::kColorId_NotificationButtonBackground)));
#endif
  SetInkDropBaseColor(theme->GetSystemColor(
      ui::NativeTheme::kColorId_PaddedButtonInkDropColor));
}

}  // namespace message_center
