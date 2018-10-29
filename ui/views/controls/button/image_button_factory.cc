// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ui/views/controls/button/image_button_factory.h"

#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/painter.h"

namespace views {

ImageButton* CreateVectorImageButton(ButtonListener* listener) {
  ImageButton* button = new ImageButton(listener);
  button->SetInkDropMode(Button::InkDropMode::ON);
  button->set_has_ink_drop_action_on_click(true);
  button->SetImageAlignment(ImageButton::ALIGN_CENTER,
                            ImageButton::ALIGN_MIDDLE);
  button->SetFocusPainter(nullptr);
  button->SetBorder(CreateEmptyBorder(
      LayoutProvider::Get()->GetInsetsMetric(INSETS_VECTOR_IMAGE_BUTTON)));
  return button;
}

void SetImageFromVectorIcon(ImageButton* button,
                            const gfx::VectorIcon& icon,
                            SkColor related_text_color) {
  const SkColor icon_color =
      color_utils::DeriveDefaultIconColor(related_text_color);
  const SkColor disabled_color =
      SkColorSetA(icon_color, gfx::kDisabledControlAlpha);
  button->SetImage(Button::STATE_NORMAL,
                   gfx::CreateVectorIcon(icon, icon_color));
  button->SetImage(Button::STATE_DISABLED,
                   gfx::CreateVectorIcon(icon, disabled_color));
  button->set_ink_drop_base_color(icon_color);
}

}  // views
