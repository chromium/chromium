// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/padded_button.h"

#include <memory>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"

namespace message_center {

PaddedButton::PaddedButton(PressedCallback callback)
    : views::ImageButton(std::move(callback)) {
  SetBorder(views::CreateEmptyBorder(kControlButtonBorderSize));
  SetAnimateOnStateChange(false);

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InkDrop::Get(this)->SetVisibleOpacity(0.12f);
  SetHasInkDropActionOnClick(true);
  views::InkDrop::UseInkDropForSquareRipple(views::InkDrop::Get(this),
                                            /*highlight_on_hover=*/false);
}

void PaddedButton::OnThemeChanged() {
  ImageButton::OnThemeChanged();
  const auto* color_provider = GetColorProvider();
  SkColor background_color =
      color_provider->GetColor(ui::kColorWindowBackground);
  views::InkDrop::Get(this)->SetBaseColor(
      color_utils::GetColorWithMaxContrast(background_color));
}

BEGIN_METADATA(PaddedButton)
END_METADATA

}  // namespace message_center
